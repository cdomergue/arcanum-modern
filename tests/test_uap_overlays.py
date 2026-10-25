#!/usr/bin/env python3
"""Exercise production FR/EN repository mounting, precedence and rollback."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/game/modern_uap.c').read_text()
source='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#define TIG_MAX_PATH 260
#define SDL_arraysize(a) (sizeof(a)/sizeof((a)[0]))
#define SDL_strcasecmp strcasecmp
#define tig_debug_printf(...) ((void)0)
typedef FILE TigFile;
static void SDL_strlcpy(char* dst,const char* src,size_t n){snprintf(dst,n,"%s",src);}
static bool installed;
static int added,removed,fail_at=-1;
static char paths[16][260],removed_paths[16][260];
static bool tig_file_exists(const char* p,void* unused){(void)unused;assert(!strcmp(p,"Arcanum5.dat"));return installed;}
static bool tig_file_repository_add(const char* p){assert(added<16);snprintf(paths[added],260,"%s",p);return added++!=fail_at;}
static void tig_file_repository_remove(const char* p){assert(removed<16);snprintf(removed_paths[removed++],260,"%s",p);}
static const char* gamelib_current_module_name_get(void){return "Arcanum";}
static TigFile* tig_file_fopen_repository(const char* repo,const char* path){(void)repo;(void)path;return NULL;}
void modern_uap_module_unload(void);
'''+source+r'''
static void marker(const char* lang,const char* text){char path[260];snprintf(path,260,"modern-ui/uap-%s",lang);mkdir(path,0700);snprintf(path,260,"modern-ui/uap-%s/active",lang);mkdir(path,0700);snprintf(path,260,"modern-ui/uap-%s/active/ready",lang);FILE* f=fopen(path,"wb");assert(f);fputs(text,f);fclose(f);}
int main(void){
 mkdir("modern-ui",0700);assert(!modern_uap_ready());
 marker("en","partial");assert(!modern_uap_ready());
 for(int french=0;french<2;french++){
  const char* lang=french?"fr":"en";marker(lang,french?"2.0.2-fr-1\n":"2.0.2-en-1\n");
  assert(modern_uap_ready());installed=true;assert(!modern_uap_ready());installed=false;
  added=removed=0;assert(modern_uap_load());assert(added==(french?8:7));
  for(int i=0;i<added;i++) {assert(strstr(paths[i],french?"uap-fr/":"uap-en/"));if(!french)assert(!strstr(paths[i],"french"));}
  assert(strstr(paths[added-1],"/data"));
  added=removed=0;assert(modern_uap_module_load("Arcanum"));assert(added==(french?3:2));
  assert(strstr(paths[0],"arcanum.patch1") && strstr(paths[1],"arcanum.patch9"));
  modern_uap_module_unload();assert(removed==added);
  for(int i=0;i<added;i++)assert(!strcmp(removed_paths[i],paths[added-1-i]));
  added=removed=0;assert(modern_uap_module_load("Vormantown"));assert(added==1&&strstr(paths[0],"vormantown.patch9"));modern_uap_module_unload();
  added=removed=0;assert(modern_uap_module_load("Other"));assert(!added);
  added=removed=0;fail_at=1;assert(!modern_uap_module_load("Arcanum"));assert(removed==1&&!strcmp(paths[0],removed_paths[0]));fail_at=-1;
  added=removed=0;fail_at=french?7:6;assert(!modern_uap_load());assert(removed==fail_at);assert(!modern_uap_ready());disabled=false;fail_at=-1;
  char path[260];snprintf(path,260,"modern-ui/uap-%s/active/ready",lang);unlink(path);
 }
 puts("PASS: FR/EN markers, external UAP precedence, language-specific files, module switching and failed-mount rollback");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-uap-mount-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
