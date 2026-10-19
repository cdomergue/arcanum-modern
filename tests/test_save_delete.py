#!/usr/bin/env python3
"""Check VFS deletion across missing repositories and case-sensitive save paths."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/first_party/tig/src/file.c').read_text()
a=s.index('int tig_file_remove_native(const char* file_name)\n{');body=s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define TIG_MAX_PATH 512
#define TIG_FILE_PLAIN 1
#define TIG_FILE_DATABASE 2
typedef struct {int type;} SDL_PathInfo;
typedef struct {int flags;} TigDatabaseEntry;
typedef struct Repo {int type;const char* path;struct Repo* next;int database;} TigFileRepository;
static TigFileRepository second={1,"module",0,0},first={1,"missing",&second,0};
static TigFileRepository* tig_file_repositories_head=&first;
static bool present=true,denied;
static int calls;
static unsigned tig_file_ignored(const char* f){(void)f;return 0;}
static void compat_join_path(char* p,size_t n,const char* a,const char* b){snprintf(p,n,"%s/%s",a,b);}
static void compat_resolve_path(char* p){char* s=strstr(p,"/save/");if(s && !strncmp(p,"module/",7))s[1]='S';}
static bool SDL_GetPathInfo(const char* p,SDL_PathInfo* i){(void)i;return present&&!strcmp(p,"module/Save/slot.gsi");}
static bool SDL_RemovePath(const char* p){calls++;if(!strcmp(p,"module/Save/slot.gsi")){if(denied)return false;present=false;}return true;}
static bool tig_database_get_entry(int d,const char* f,TigDatabaseEntry** e){(void)d;(void)f;(void)e;return false;}
'''+body+r'''
int main(void){
 assert(tig_file_remove_native("save/slot.gsi")==0);assert(!present&&calls==1);
 assert(tig_file_remove_native("save/slot.gsi")!=0);assert(calls==1);
 present=true;denied=true;assert(tig_file_remove_native("save/slot.gsi")!=0);assert(present);
 puts("PASS: missing repository skipped, Save/save resolved, existing file deleted, failure reported");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-delete-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
