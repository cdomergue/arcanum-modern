"""Check the production catalogue transaction with deterministic I/O failures."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
typedef int mes_file_handle_t;
#define MES_FILE_HANDLE_INVALID -1
typedef struct {int num;char* str;} MesFileEntry;
typedef struct {int count;MesFileEntry entries[1001];} Mes;
static Mes* tables[8];
static int portrait_user_mes_file;
static bool portrait_have_user_mes_file;
static int mes_entries_count(int h){return tables[h]->count;}
static bool mes_get_entry(int h,int n,MesFileEntry* e){if(n>=tables[h]->count)return false;*e=tables[h]->entries[n];return true;}
static bool mes_search(int h,MesFileEntry* e){for(int n=0;n<tables[h]->count;n++)if(tables[h]->entries[n].num==e->num){*e=tables[h]->entries[n];return true;}return false;}
static bool mes_load(const char* path,int* h){
 FILE* f=fopen(path,"rb");*h=-1;if(!f)return false;
 Mes* m=calloc(1,sizeof(*m));char line[4096];
 while(fgets(line,sizeof(line),f)){int id;char name[256];if(sscanf(line,"{%d}{%255[^}]}",&id,name)==2){assert(m->count<1001);m->entries[m->count++]=(MesFileEntry){id,strdup(name)};}}
 fclose(f);if(!m->count){free(m);return false;}
 for(int n=0;n<8;n++)if(!tables[n]){tables[n]=m;*h=n;return true;}
 abort();
}
static bool mes_unload(int h){Mes* m=tables[h];for(int n=0;n<m->count;n++)free(m->entries[n].str);free(m);tables[h]=NULL;return true;}
typedef struct {int w,h,format;} SDL_Surface;
#define SDL_PIXELFORMAT_INDEX8 1
typedef struct {size_t size;} SDL_PathInfo;
typedef FILE SDL_IOStream;
typedef FILE TigFile;
#define SDL_malloc malloc
#define SDL_free free
#define SDL_strdup strdup
#define SDL_IOFromFile fopen
#define tig_file_fopen fopen
#define tig_file_fread fread
#define tig_file_fclose fclose
static bool SDL_GetPathInfo(const char* p,SDL_PathInfo* i){struct stat s;if(stat(p,&s))return false;i->size=s.st_size;return true;}
static char* SDL_LoadFile(const char* p,size_t* length){FILE* f=fopen(p,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);*length=ftell(f);rewind(f);char* s=calloc(*length+1,1);assert(fread(s,1,*length,f)==*length);fclose(f);return s;}
static bool SDL_CreateDirectory(const char* p){mkdir("data",0700);mkdir(p,0700);return true;}
static size_t SDL_WriteIO(FILE* f,const void* p,size_t n){return fwrite(p,1,n,f);}
static bool SDL_CloseIO(FILE* f){return fclose(f)==0;}
static int tig_file_filelength(FILE* f){long pos=ftell(f);fseek(f,0,SEEK_END);int n=ftell(f);fseek(f,pos,SEEK_SET);return n;}
static uint64_t SDL_GetTicksNS(void){return 123;}
static bool SDL_CopyFile(const char* a,const char* b){size_t n;char* s=SDL_LoadFile(a,&n);FILE* f=fopen(b,"wb");bool ok=s&&f&&fwrite(s,1,n,f)==n;if(f)fclose(f);free(s);return ok;}
static bool fail_rename;static int fail_bmp,bmp_calls;
static bool SDL_RenamePath(const char* a,const char* b){return !fail_rename&&rename(a,b)==0;}
static bool SDL_RemovePath(const char* p){return remove(p)==0;}
static bool SDL_SaveBMP(SDL_Surface* s,const char* p){(void)s;FILE* f=fopen(p,"wb");assert(f);fputs("BMP",f);fclose(f);return ++bmp_calls!=fail_bmp;}
typedef enum {PORTRAIT_IMPORT_OK,PORTRAIT_IMPORT_IO,PORTRAIT_IMPORT_CATALOGUE,PORTRAIT_IMPORT_FULL} PortraitImportResult;
#include "IMPORT_SOURCE"
static int images(void){DIR* d=opendir("data/portrait");assert(d);int count=0;struct dirent* e;while((e=readdir(d)))if(strstr(e->d_name,".bmp"))count++;closedir(d);return count;}
int main(void){
 SDL_Surface small={64,64,1},large={128,128,1};int id=-1;
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_OK && id==1 && images()==2);
 const char* path="data/portrait/userport.mes";size_t size;char* before=SDL_LoadFile(path,&size);
 fail_bmp=bmp_calls+2;
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_IO && id==1 && images()==2);
 size_t after_size;char* after=SDL_LoadFile(path,&after_size);assert(size==after_size&&!memcmp(before,after,size));free(after);
 fail_bmp=0;fail_rename=true;
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_IO && images()==2);
 after=SDL_LoadFile(path,&after_size);assert(size==after_size&&!memcmp(before,after,size));free(after);fail_rename=false;
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_OK && id==2 && images()==4);
 after=SDL_LoadFile(path,&after_size);assert(after_size>size&&!memcmp(before,after,size));free(after);free(before);
 FILE* f=fopen(path,"ab");fputs("\n{3}{ANYexternal}\n",f);fclose(f);
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_CATALOGUE && images()==4);
 mes_unload(portrait_user_mes_file);assert(mes_load(path,&portrait_user_mes_file));
 tables[portrait_user_mes_file]->entries[2].num=4;
 assert(portrait_import(&small,&large,&id)==PORTRAIT_IMPORT_CATALOGUE);
 mes_unload(portrait_user_mes_file);portrait_have_user_mes_file=false;
 puts("PASS: catalogue preservation, immutable IDs, partial BMP failure cleanup, atomic publication failure, external edits and numbering gaps");
}
'''
with tempfile.TemporaryDirectory(prefix="arcanum-portrait-catalogue-") as work:
    path = Path(work)
    path.joinpath("test.c").write_text(SOURCE.replace("IMPORT_SOURCE", (ROOT / "source/src/game/modern_portrait_import.inc").as_posix()))
    subprocess.run([os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra", "-Werror", "-fsanitize=undefined", str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], cwd=path, check=True)
