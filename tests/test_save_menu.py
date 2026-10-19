#!/usr/bin/env python3
"""Exercise shared save/load indexing and safe native save-name editing."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/mainmenu_ui.c').read_text()
body=s[s.index('static bool modern_mainmenu_save_screen(void)\n{'):s.index('const char* modern_mainmenu_save_location(')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
enum {MM_WINDOW_LOAD_GAME,MM_WINDOW_SAVE_GAME,MM_WINDOW_OTHER};
typedef struct {int selected_index;} MainMenuWindowInfo;
typedef struct {int marker;} GameSaveInfo;
typedef struct {int num;const char* str;} MesFileEntry;
static int mainmenu_ui_window_type,mainmenu_ui_mainmenu_mes_file;
static MainMenuWindowInfo windows[3],*main_menu_window_info[]={windows,windows+1,windows+2};
static struct {unsigned count;const char* names[2];} mainmenu_ui_gsl={2,{"Slot0000First","Slot0001Second"}};
static char mainmenu_ui_textedit_buffer[128];
static bool mainmenu_ui_gsi_loaded;
static GameSaveInfo mainmenu_ui_gsi;
static int loaded=-1,releases;
static void mes_get_msg(int f,MesFileEntry* e){(void)f;assert(e->num==5055);e->str="New save";}
static const char* sub_543040(int i){assert(i>=0&&i<2);return mainmenu_ui_gsl.names[i]+8;}
static void gamelib_saveinfo_exit(GameSaveInfo* p){(void)p;releases++;}
static bool gamelib_saveinfo_load(const char* name,GameSaveInfo* p){(void)p;loaded=name==mainmenu_ui_gsl.names[0]?0:1;return true;}
static void mainmenu_ui_save_game_refresh(void* p){(void)p;}
static void mainmenu_ui_load_game_refresh(void* p){(void)p;}
'''+body+r'''
int main(void){
 mainmenu_ui_window_type=MM_WINDOW_LOAD_GAME;
 assert(!strcmp(modern_mainmenu_save_name(0),"First"));
 modern_mainmenu_save_select(1);assert(loaded==1&&windows[0].selected_index==1&&modern_mainmenu_save_info());
 modern_mainmenu_save_select(2);assert(loaded==1&&windows[0].selected_index==1);
 mainmenu_ui_window_type=MM_WINDOW_SAVE_GAME;
 assert(!strcmp(modern_mainmenu_save_name(0),"New save"));
 assert(!strcmp(modern_mainmenu_save_name(1),"First"));
 assert(!strcmp(modern_mainmenu_save_name(2),"Second"));
 assert(!*modern_mainmenu_save_name(3));
 modern_mainmenu_save_select(1);assert(loaded==0&&windows[1].selected_index==1);
 modern_mainmenu_save_select(0);assert(!modern_mainmenu_save_info()&&releases==2);
 modern_mainmenu_save_description_set("Tarant \xe9t\xe9 /:*?<>|\\\"'\n");
 assert(!strcmp(modern_mainmenu_save_description(),"Tarant \xe9t\xe9 "));
 modern_mainmenu_save_description_set("12345678901234567890123456789");assert(strlen(modern_mainmenu_save_description())==23);
 modern_mainmenu_save_select(2);modern_mainmenu_save_description_set("ignored");assert(strlen(modern_mainmenu_save_description())==23);
 modern_mainmenu_save_select(0);modern_mainmenu_save_description_set("");assert(!*modern_mainmenu_save_description());
 mainmenu_ui_gsl.count=0;modern_mainmenu_save_select(0);assert(windows[1].selected_index==0&&!modern_mainmenu_save_info());
 mainmenu_ui_window_type=MM_WINDOW_OTHER;modern_mainmenu_save_select(0);assert(!*modern_mainmenu_save_name(0));
 puts("PASS: new slot, existing slot offsets, empty list, preview lifetime, accented/path-safe names and length limit");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-save-menu-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
