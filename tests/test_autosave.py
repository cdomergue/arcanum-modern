#!/usr/bin/env python3
"""Run the autosave controller against an engine/filesystem fault-injection harness."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'source/src/game/modern_autosave.c').read_text()
source = source[source.index('#define AUTOSAVE_GENERATIONS'):]
harness = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define TIG_MAX_PATH 260
#define OBJ_HANDLE_NULL 0
#define INTGAME_MODE_MAIN 0
typedef unsigned tig_timestamp_t;
typedef struct {char path[TIG_MAX_PATH];} TigFileInfo;
typedef struct {unsigned count;TigFileInfo* entries;} TigFileList;
typedef struct {unsigned count;char** names;} GameSaveList;
typedef struct {int num;char* str;} MesFileEntry;
static char files[256][TIG_MAX_PATH];
static int file_count,write_count,rename_count,fail_rename,fail_save;
static bool reset_state,load_state,save_state,menu,worldmap,dialog,teleport,combat,dead,unconscious,armed;
static int mode,map_id=1;
static int64_t pc=1;
static unsigned now;
static void add_file(const char* name){assert(file_count<256);strcpy(files[file_count++],name);}
static int find_file(const char* name){for(int i=0;i<file_count;i++)if(!strcmp(files[i],name))return i;return -1;}
static int tig_file_remove(const char* name){int i=find_file(name);if(i<0)return 1;memmove(files+i,files+i+1,(--file_count-i)*sizeof(files[0]));return 0;}
static int tig_file_rename(const char* old,const char* name){if(++rename_count==fail_rename)return 1;int i=find_file(old);assert(i>=0);assert(find_file(name)<0);strcpy(files[i],name);return 0;}
static void tig_file_list_create(TigFileList* list,const char* pattern){size_t n=strlen(pattern)-1;list->count=0;list->entries=calloc(256,sizeof(*list->entries));for(int i=0;i<file_count;i++)if(!strncmp(pattern,files[i],n))strcpy(list->entries[list->count++].path,files[i]+5);}
static void tig_file_list_destroy(TigFileList* list){free(list->entries);}
static void gamelib_savelist_create(GameSaveList* list){list->count=0;list->names=calloc(256,sizeof(char*));for(int i=0;i<file_count;i++){size_t n=strlen(files[i]);if(!strncmp(files[i],"save\\Slot",9)&&n>4&&!strcmp(files[i]+n-4,".gsi")){list->names[list->count]=strdup(files[i]+5);list->names[list->count++][n-9]=0;}}}
static void gamelib_savelist_destroy(GameSaveList* list){for(unsigned i=0;i<list->count;i++)free(list->names[i]);free(list->names);}
static bool gamelib_delete(const char* name){char path[260];snprintf(path,sizeof(path),"save\\%s.gsi",name);if(tig_file_remove(path))return false;const char* extensions[]={".tfaf",".tfai",".bmp"};for(int i=0;i<3;i++){snprintf(path,sizeof(path),"save\\%.8s%s",name,extensions[i]);tig_file_remove(path);}return true;}
static bool gamelib_save(const char* name,const char* description){write_count++;char path[260];const char* extensions[]={".tfaf",".tfai",".bmp"};for(int i=0;i<3;i++){snprintf(path,sizeof(path),"save\\%s%s",name,extensions[i]);add_file(path);}snprintf(path,sizeof(path),"save\\%s%s.gsi",name,description);add_file(path);return !fail_save;}
static void mes_get_msg(int handle,MesFileEntry* entry){(void)handle;assert(entry->num==5056);entry->str="Enregistrement rapide";}
static int mainmenu_ui_mes_file(void){return 1;}
static bool gamelib_in_reset(void){return reset_state;}
static bool gamelib_in_load(void){return load_state;}
static bool gamelib_in_save(void){return save_state;}
static bool mainmenu_ui_is_active(void){return menu;}
static int intgame_mode_get(void){return mode;}
static bool wmap_ui_is_created(void){return worldmap;}
static bool dialog_ui_is_local_pc_in_dialog(void){return dialog;}
static bool teleport_is_teleporting(void){return teleport;}
static bool combat_turn_based_is_active(void){return combat;}
static int64_t player_get_local_pc_obj(void){return pc;}
static int map_current_map(void){return map_id;}
static bool critter_is_dead(int64_t obj){(void)obj;return dead;}
static bool critter_is_unconscious(int64_t obj){(void)obj;return unconscious;}
static bool combat_critter_is_combat_mode_active(int64_t obj){(void)obj;return armed;}
static void tig_timer_now(unsigned* time){*time=now;}
static unsigned tig_timer_elapsed(unsigned time){return now-time;}
static void tig_debug_printf(const char* fmt,...){(void)fmt;}
'''
tests = r'''
static int autosaves(void){GameSaveList saves;int count=0;gamelib_savelist_create(&saves);for(unsigned i=0;i<saves.count;i++)if(modern_autosave_is_slot(saves.names[i]))count++;gamelib_savelist_destroy(&saves);return count;}
static void assert_complete(void){GameSaveList saves;gamelib_savelist_create(&saves);for(unsigned i=0;i<saves.count;i++){char path[260];const char* ext[]={".tfaf",".tfai",".bmp"};for(int j=0;j<3;j++){snprintf(path,sizeof(path),"save\\%.8s%s",saves.names[i],ext[j]);assert(find_file(path)>=0);}}gamelib_savelist_destroy(&saves);}
int main(void){
 assert(!modern_autosave_is_slot("SlotAutoAuto-Save"));assert(!modern_autosave_is_slot("Slot0001Manual"));assert(!modern_autosave_is_slot("SlotA"));assert(modern_autosave_is_slot("SlotA001Label"));
 gamelib_save("Slot0012","Manual");int manual_files=file_count;write_count=0;
 for(int i=0;i<8;i++){modern_autosave_reset();modern_autosave_before_map_change();assert(autosaves()==(i<3?i+1:3));assert_complete();}
 assert(find_file("save\\Slot0012Manual.gsi")>=0);assert(file_count==manual_files+12);
 for(int fail=0;fail<=4;fail++){
  char snapshot[256][TIG_MAX_PATH];int old_count=file_count;memcpy(snapshot,files,sizeof(files));fail_save=fail==0;fail_rename=fail;rename_count=0;
  modern_autosave_before_map_change();assert(file_count==old_count);for(int i=0;i<old_count;i++)assert(find_file(snapshot[i])>=0);assert_complete();
 }
 fail_save=fail_rename=0;
 bool* guards[]={&reset_state,&load_state,&save_state,&menu,&worldmap,&dialog,&teleport,&combat,&dead,&unconscious,&armed};
 for(unsigned i=0;i<sizeof(guards)/sizeof(guards[0]);i++){int count=write_count;*guards[i]=true;modern_autosave_before_map_change();assert(write_count==count);*guards[i]=false;}
 int count=write_count;pc=0;modern_autosave_before_map_change();pc=1;mode=1;modern_autosave_before_map_change();mode=0;assert(write_count==count);
 modern_autosave_after_combat();modern_autosave_tick();assert(write_count==count);now+=1000;menu=true;modern_autosave_tick();assert(write_count==count);menu=false;modern_autosave_tick();assert(write_count==count+1);modern_autosave_tick();assert(write_count==count+1);
 modern_autosave_after_combat();modern_autosave_reset();now+=1000;modern_autosave_tick();assert(write_count==count+1);
 load_state=true;modern_autosave_after_combat();load_state=false;now+=1000;modern_autosave_tick();assert(write_count==count+1);
 file_count=0;gamelib_save("SlotAZZY","Tail");gamelib_save("SlotAZZZ","Tail");modern_autosave_before_map_change();modern_autosave_before_map_change();assert(autosaves()==3);assert(find_file("save\\SlotAZZYTail.gsi")<0);assert(find_file("save\\SlotAZZZTail.gsi")>=0);assert_complete();
 puts("PASS: persistent rotation, wrap, manual preservation, save/rename failures, safe-state guards, deferred combat and load/reset cancellation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-autosave-') as folder:
    directory = Path(folder)
    (directory / 'test.c').write_text(harness + source + tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(directory / 'test.c'), '-o', str(directory / 'test')], check=True)
    subprocess.run([str(directory / 'test')], check=True)

archive_source = (ROOT / 'source/first_party/tig/src/file.c').read_text()
start = archive_source.index('bool tig_file_archive_native(const char* dst, const char* src)\n{')
archive_body = archive_source[start:archive_source.index('// 0x', start)]
archive_harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define TIG_MAX_PATH 260
#define SDL_snprintf snprintf
typedef int TigFile;
static int handles[2],opened,closed,removed,fail_close;
static bool tig_file_is_directory(const char* path){(void)path;return true;}
static TigFile* tig_file_fopen_native(const char* path,const char* mode){(void)path;(void)mode;return &handles[opened++];}
static bool tig_file_archive_worker_native(const char* src,TigFile* index,TigFile* data){(void)src;(void)index;(void)data;return true;}
static int tig_file_fwrite(const void* p,int size,int count,TigFile* stream){(void)p;(void)size;(void)count;(void)stream;return 1;}
static int tig_file_fclose(TigFile* stream){(void)stream;return ++closed==fail_close?-1:0;}
static int tig_file_remove_native(const char* path){(void)path;removed++;return 0;}
'''
archive_tests = r'''
int main(void){for(int i=0;i<3;i++){opened=closed=removed=0;fail_close=i;bool success=tig_file_archive_native("staged","current");assert(success==(i==0));assert(opened==2&&closed==2);assert(removed==(i==0?0:2));}puts("PASS: archive buffered-close failures are reported and both streams closed");}
'''
menu_source = (ROOT / 'source/src/ui/mainmenu_ui.c').read_text()
start = menu_source.index('    if (v1 > 0 && modern_autosave_is_slot(')
menu_body = menu_source[start:menu_source.index('    sub_542200();', start)]
menu_harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static struct {unsigned count;const char* names[5];} mainmenu_ui_gsl;
static char mainmenu_ui_textedit_buffer[128];
static bool modern_autosave_is_slot(const char* name){return !strncmp(name,"SlotA00",7);}
static bool mainmenu_ui_confirm(int num){(void)num;return false;}
static bool choose(int v1,char* output){char fname[128];const char* name;int num;
'''
menu_tests = r'''
strcpy(output,name);return true;}
int main(void){char output[128];mainmenu_ui_gsl.count=5;mainmenu_ui_gsl.names[0]="SlotA002Auto";mainmenu_ui_gsl.names[1]="SlotA001Auto";mainmenu_ui_gsl.names[2]="SlotAutoAuto-Save";mainmenu_ui_gsl.names[3]="Slot0010123";mainmenu_ui_gsl.names[4]="Slot0009Manual";
assert(choose(0,output)&&!strcmp(output,"Slot0011"));assert(choose(1,output)&&!strcmp(output,"Slot0011"));assert(choose(5,output)&&!strcmp(output,"Slot0009"));mainmenu_ui_gsl.count=2;assert(choose(0,output)&&!strcmp(output,"Slot0000"));puts("PASS: manual allocation ignores every autosave and preserves four-digit slot IDs");}
'''
for label, code in [('archive', archive_harness + archive_body + archive_tests), ('menu', menu_harness + menu_body + menu_tests)]:
    with tempfile.TemporaryDirectory(prefix='arcanum-autosave-' + label + '-') as folder:
        directory = Path(folder)
        (directory / 'test.c').write_text(code)
        subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(directory / 'test.c'), '-o', str(directory / 'test')], check=True)
        subprocess.run([str(directory / 'test')], check=True)
