#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
file_source = (ROOT / 'source/first_party/tig/src/file.c').read_text()
compat_source = (ROOT / 'source/first_party/tig/src/compat.c').read_text()

def function(source, signature):
    start = source.index(signature + '\n{')
    return source[start:source.index('\n}', start) + 2]

code = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#define TIG_MAX_PATH 512
#define TIG_FILE_PLAIN 1
#define SDL_strncasecmp strncasecmp
typedef struct Repo {int type;const char* path;struct Repo* next;} TigFileRepository;
static TigFileRepository module={1,"Module",NULL},missing={1,"Missing",&module};
static TigFileRepository* tig_file_repositories_head=&missing;
static unsigned tig_file_ignored(const char* path){(void)path;return 0;}
static void compat_join_path(char* out,size_t size,const char* a,const char* b){snprintf(out,size,"%s/%s",a,b);}
static bool SDL_RenamePath(const char* from,const char* to){return rename(from,to)==0;}
''' + function(compat_source, 'void compat_resolve_path(char* path)') + '\n' + function(
    file_source, 'int tig_file_rename_native(const char* old_file_name, const char* new_file_name)') + r'''
static void write_file(const char* path,const char* value){FILE* f=fopen(path,"wb");assert(f);assert(fputs(value,f)>=0);assert(!fclose(f));}
static void expect_file(const char* path,const char* value){char data[64]={0};FILE* f=fopen(path,"rb");assert(f);assert(fread(data,1,sizeof(data)-1,f)==strlen(value));assert(!strcmp(data,value));assert(!fclose(f));}
int main(void){
 assert(!mkdir("Module",0700));assert(!mkdir("Module/Save",0700));
 write_file("Module/Save/Slot0000Manual.gsi","manual");
 const char* ext[]={".tfaf",".tfai",".bmp","Autosave.gsi"};
 for(int i=0;i<4;i++){
  char real[256],from[256],to[256];
  snprintf(real,sizeof(real),"Module/Save/MdrnAuto%s",ext[i]);write_file(real,"staged");
  snprintf(from,sizeof(from),"save/MdrnAuto%s",ext[i]);
  snprintf(to,sizeof(to),"save/SlotA000%s",ext[i]);
  assert(tig_file_rename_native(from,to)==0);
  assert(access(real,F_OK)!=0);
  snprintf(real,sizeof(real),"Module/Save/SlotA000%s",ext[i]);expect_file(real,"staged");
 }
 assert(tig_file_rename_native("save/Absent.tfai","save/SlotA001.tfai")!=0);
 expect_file("Module/Save/SlotA000.tfai","staged");
 expect_file("Module/Save/Slot0000Manual.gsi","manual");
 write_file("direct.tmp","direct");assert(tig_file_rename_native("./direct.tmp","./direct.done")==0);expect_file("direct.done","direct");
 puts("PASS: staged autosave files published through Save/save paths; missing repository skipped; missing source and manual save preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-rename-') as folder:
    directory = Path(folder)
    (directory / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(directory / 'test.c'), '-o', str(directory / 'test')], check=True)
    subprocess.run([str(directory / 'test')], cwd=directory, check=True)
