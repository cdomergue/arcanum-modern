#!/usr/bin/env python3
"""Check companion ownership guards and isolation of the player's spell hotkeys."""
from pathlib import Path
import os, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
def function(file, name):
    source = (root/file).read_text()
    match = re.search(r'^(?:bool|void) '+name+r'\([^\n]+\)\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index('\n}', match.end())+2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_TYPE 1
#define OBJ_F_FLAGS 2
#define OBJ_TYPE_NPC 1
#define OF_DESTROYED 1
#define OF_EXTINCT 2
#define CHAREDIT_MODE_PASSIVE 2
#define STAT_UNSPENT_POINTS 0
static int64_t pc=1,leader=1,charedit_obj=2;
static int type=OBJ_TYPE_NPC,flags,scheme,charedit_mode=1;
static bool charedit_created=true,removed=true;
static int points[3]={0,7,3},hotkeys,refreshes;
static int64_t player_get_local_pc_obj(void){return pc;}
static bool player_is_local_pc_obj(int64_t obj){return obj==pc;}
static bool obj_handle_is_valid(int64_t obj){return obj==1||obj==2;}
static int obj_field_int32_get(int64_t obj,int field){assert(obj==2);return field==OBJ_F_TYPE?type:flags;}
static int64_t critter_pc_leader_get(int64_t obj){assert(obj==2);return leader;}
static int auto_level_scheme_get(int64_t obj){assert(obj==2);return scheme;}
static int spell_cost(int spell){assert(spell==3);return 1;}
static int stat_level_get(int64_t obj,int stat){assert(stat==STAT_UNSPENT_POINTS);return points[obj];}
static bool spell_remove(int64_t obj,int spell){assert((obj==1||obj==2)&&spell==3);return removed;}
static void hotkey_ui_notify_spell_removed(int spell){assert(spell==3);hotkeys++;}
static void stat_base_set(int64_t obj,int stat,int value){assert(stat==STAT_UNSPENT_POINTS);points[obj]=value;}
static bool charedit_is_created(void){return charedit_created;}
static void charedit_refresh(void){refreshes++;}
static void intgame_message_window_clear(void){}
''' + '\n'.join([
    function('source/src/ui/charedit_ui.c','modern_charedit_manual_follower'),
    function('source/src/ui/charedit_ui.c','modern_charedit_editable'),
    function('source/src/ui/spell_ui.c','spell_ui_remove'),
]) + r'''
int main(void){
 assert(modern_charedit_manual_follower(2)&&modern_charedit_editable());
 assert(!modern_charedit_manual_follower(0)&&!modern_charedit_manual_follower(1)&&!modern_charedit_manual_follower(99));
 scheme=50;assert(!modern_charedit_manual_follower(2)&&!modern_charedit_editable());scheme=0;
 leader=3;assert(!modern_charedit_manual_follower(2));leader=1;
 flags=OF_DESTROYED;assert(!modern_charedit_manual_follower(2));flags=OF_EXTINCT;assert(!modern_charedit_manual_follower(2));flags=0;
 type=0;assert(!modern_charedit_manual_follower(2));type=OBJ_TYPE_NPC;
 pc=0;assert(!modern_charedit_manual_follower(2));pc=1;
 charedit_mode=CHAREDIT_MODE_PASSIVE;assert(!modern_charedit_editable());charedit_mode=1;
 charedit_created=false;assert(!modern_charedit_editable());charedit_created=true;
 charedit_obj=1;assert(modern_charedit_editable());charedit_obj=2;
 spell_ui_remove(2,3);assert(points[2]==4&&points[1]==7&&hotkeys==0&&refreshes==1);
 removed=false;spell_ui_remove(2,3);assert(points[2]==4&&hotkeys==0&&refreshes==1);
 removed=true;spell_ui_remove(1,3);assert(points[1]==8&&hotkeys==1&&refreshes==2);
 puts("PASS: ownership, no-scheme, live-object and read-only guards; companion spell refund preserves player hotkeys");
}
'''
with tempfile.TemporaryDirectory(prefix='manual-companion-') as folder:
    p = Path(folder)
    (p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
