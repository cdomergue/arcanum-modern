#!/usr/bin/env python3
"""Exercise native town-note serialization, owner switching and marking order."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1];source=(root/'source/src/ui/wmap_ui.c').read_text()
def fn(signature):
 start=source.index(signature+'\n{');return source[start:source.index('\n}',start)+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
typedef uint64_t Uint64;
typedef FILE TigFile;
#define TIG_MAX_PATH 512
static int tig_file_fread(void* p,int size,int count,FILE* f){return (int)fread(p,(size_t)size,(size_t)count,f);}
static int tig_file_fwrite(const void* p,int size,int count,FILE* f){return (int)fwrite(p,(size_t)size,(size_t)count,f);}
#define tig_file_fclose fclose
#define tig_debug_printf(...) ((void)0)
#define WMAP_UI_MODE_TOWN 2
typedef int WmapUiMode;
static int wmap_ui_mode=2,wmap_ui_townmap=205;
#define WMAP_NOTE_TYPE_QUEST 3
#define OBJ_F_LOCATION 0
#define OBJ_HANDLE_NULL 0
static bool fail_write,fail_read;
static TigFile* tig_file_fopen(const char* path,const char* mode){if((fail_write&&mode[0]=='w') || (fail_read&&mode[0]=='r')){errno=EACCES;return NULL;}char p[512];snprintf(p,sizeof(p),"%s",path);for(char* c=p;*c;c++)if(*c=='\\')*c='/';return fopen(p,mode);}
static bool tig_file_exists(const char* path,void* unused){(void)unused;bool failed=fail_read;fail_read=false;FILE* f=tig_file_fopen(path,"rb");fail_read=failed;if(f){fclose(f);return true;}return false;}
typedef struct{int x,y;}WmapCoords;
typedef struct{int x,y,width,height;}TigRect;
typedef struct{int id;unsigned flags;WmapCoords coords;int64_t field_10;TigRect field_18;int type;char str[250];void* video_buffer;}WmapNote;
typedef struct{int field_194;}WmapInfo;
static WmapNote wmap_ui_town_notes[200];
static WmapInfo wmap_ui_mode_info[3];
static int dword_66D878,wmap_ui_num_town_notes,active_city=101,wmap_ui_tmi;
static Uint64 ticks=1000;
static Uint64 SDL_GetTicks(void){return ticks;}
static const char* townmap_name(int id){return id==101?"city-a":id==205?"city-b":id==309?"city-c":NULL;}
static int64_t player_get_local_pc_obj(void){return 1;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)field;return obj;}
static int64_t sector_id_from_loc(int64_t loc){(void)loc;return active_city;}
static int townmap_get(int64_t sec){return (int)sec;}
static bool townmap_info(int city,int* tmi){*tmi=city;return true;}
static void townmap_loc_to_coords(int* tmi,int64_t loc,int* x,int* y){(void)tmi;(void)loc;*x=10;*y=20;}
static bool find_note_by_coords_mode(WmapCoords* coords,int* out,int mode){(void)out;(void)mode;for(int i=0;i<wmap_ui_num_town_notes;i++)if(wmap_ui_town_notes[i].coords.x==coords->x&&wmap_ui_town_notes[i].coords.y==coords->y)return true;return false;}
static void object_examine(int64_t obj,int64_t other,char* out){(void)obj;(void)other;strcpy(out,active_city==101?"Auto A":"Auto B");}
static void sub_563D50(WmapNote* note){(void)note;}
static int freed;
static void sub_5642F0(WmapNote* note){if(note->flags&4){freed++;note->flags&=~4;}}
static bool wmap_note_add_mode(WmapNote* note,int mode){(void)mode;note->id=wmap_ui_mode_info[2].field_194++;wmap_ui_town_notes[wmap_ui_num_town_notes++]=*note;return true;}
static bool wmap_ui_rect_write(TigRect* rect,TigFile* stream);
static bool wmap_ui_rect_read(TigRect* rect,TigFile* stream);
static bool wmap_ui_town_note_save(WmapNote* note,TigFile* stream);
static bool wmap_ui_town_note_load(WmapNote* note,TigFile* stream);
static bool wmap_ui_town_notes_save(void);
static bool sub_5650C0(void);
'''
code+=(root/'source/src/ui/modern_map_notes.inc').read_text()
for signature in ['bool wmap_ui_rect_write(TigRect* rect, TigFile* stream)','bool wmap_ui_rect_read(TigRect* rect, TigFile* stream)','bool wmap_ui_town_note_save(WmapNote* note, TigFile* stream)','bool wmap_ui_town_note_load(WmapNote* note, TigFile* stream)','bool wmap_ui_town_notes_save(void)','bool sub_5650C0(void)','void wmap_ui_mark_townmap(int64_t obj)','bool wmap_note_add(WmapNote* note)']:
 code+=fn(signature)
code+=r'''
int main(void){
 wmap_ui_mark_townmap(9);assert(dword_66D878==101&&wmap_ui_num_town_notes==1);
 WmapNote manual={0};strcpy(manual.str,"Personal A");manual.coords=(WmapCoords){60,70};wmap_note_add_mode(&manual,2);
 /* A new mark can arrive before the sector-change callback. A and B share
    marker coordinates; deduplication must run after choosing B's collection. */
 active_city=205;wmap_ui_mark_townmap(9);assert(dword_66D878==205&&wmap_ui_num_town_notes==1);assert(!strcmp(wmap_ui_town_notes[0].str,"Auto B"));
 strcpy(manual.str,"Personal B");wmap_note_add_mode(&manual,2);
 assert(wmap_ui_town_notes_select(205));assert(wmap_ui_num_town_notes==2); /* no reload of unsaved edits */
 wmap_ui_town_notes[0].flags|=4;wmap_ui_town_notes[0].video_buffer=(void*)1;
 assert(wmap_ui_town_notes_select(101));assert(freed==1&&wmap_ui_num_town_notes==2);assert(!strcmp(wmap_ui_town_notes[1].str,"Personal A"));
 assert(wmap_ui_town_notes_select(205));assert(wmap_ui_num_town_notes==2);assert(!strcmp(wmap_ui_town_notes[1].str,"Personal B"));assert(!(wmap_ui_town_notes[0].flags&4)&&!wmap_ui_town_notes[0].video_buffer);
 /* Reset before loading another session in the same city must force a reload. */
 dword_66D878=0;wmap_ui_num_town_notes=0;assert(wmap_ui_town_notes_select(205));assert(wmap_ui_num_town_notes==2);
 assert(wmap_ui_town_notes_select(309));assert(dword_66D878==309&&!wmap_ui_num_town_notes);
 assert(wmap_ui_town_notes_select(101));assert(wmap_ui_num_town_notes==2);
 fail_write=true;assert(!wmap_note_add(&manual));assert(!wmap_ui_town_notes_select(205));assert(dword_66D878==101&&wmap_ui_num_town_notes==2);assert(!strcmp(wmap_ui_town_notes[1].str,"Personal A"));fail_write=false;
 fail_read=true;assert(!wmap_note_add(&manual));assert(!wmap_ui_town_notes_select(205));assert(!dword_66D878&&!wmap_ui_num_town_notes);fail_read=false;
 assert(wmap_ui_town_notes_select(101));assert(wmap_ui_num_town_notes==2);
 /* Invalid headers are not published and do not overwrite the unreadable file. */
 FILE* bad=fopen("Save/Current/city-c.tmn","wb");assert(bad);int count=201;fwrite(&count,sizeof(count),1,bad);fclose(bad);
 assert(!wmap_ui_town_notes_select(309));assert(!dword_66D878&&!wmap_ui_num_town_notes);sub_5650C0();
 bad=fopen("Save/Current/city-c.tmn","rb");assert(bad);fread(&count,sizeof(count),1,bad);fclose(bad);assert(count==201);
 FILE* f=fopen("ArcanumModern/map-notes.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);ticks+=2000;
 for(int i=0;i<100;i++){errno=EDOM;town_notes_log("test",101,205,2,"loaded");assert(errno==EDOM);}
 puts("PASS: city A/B/A marking, same-coordinate isolation, unsaved personal annotations, same-city session reload, sparse IDs, runtime buffer cleanup, invalid files and bounded diagnostics");
}
'''
reset=source[source.index('void wmap_ui_reset(void)\n{'):source.index('// 0x560280')]
assert reset.index('dword_66D878 = 0')<reset.index('wmap_ui_close()')
with tempfile.TemporaryDirectory(prefix='arcanum-map-notes-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'Save/Current').mkdir(parents=True);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/map-notes.previous.log').stat().st_size==1024*1024
 log=(p/'ArcanumModern/map-notes.log').read_text();assert len(log.splitlines())==20 and 'Personal' not in log and 'Auto' not in log
