#!/usr/bin/env python3
"""Check party-menu commands cannot bypass turn ownership or native restrictions."""
from pathlib import Path
import os, re, subprocess, tempfile
source=(Path(__file__).resolve().parents[1]/'source/src/ui/follower_ui.c').read_text()
def function(name):
    match=re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    assert match,name
    return source[match.start():source.index('\n}',match.end())+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {FOLLOWER_UI_COMMAND_WALK,FOLLOWER_UI_COMMAND_ATTACK,FOLLOWER_UI_COMMAND_STAY_CLOSE,FOLLOWER_UI_COMMAND_SPREAD_OUT,FOLLOWER_UI_COMMAND_BACK_OFF,FOLLOWER_UI_COMMAND_WAIT,FOLLOWER_UI_COMMAND_INVENTORY,FOLLOWER_UI_COMMAND_CHARACTER_SHEET,FOLLOWER_UI_COMMAND_COUNT};
#define OBJ_F_SPELL_FLAGS 1
#define OSF_MIND_CONTROLLED 1
#define INVEN_UI_MODE_INVENTORY 9
#define OBJ_HANDLE_NULL 0
static bool follower_ui_initialized=true,combat,ready=true,dialog,barter=true,mind;
static int64_t actor=2,follower_ui_commander_obj,follower_ui_subordinate_obj;
static int dispatched=-1,opened;
static int64_t player_get_local_pc_obj(void){return 1;}
static bool obj_handle_is_valid(int64_t obj){return obj>=1&&obj<=4;}
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static int64_t critter_pc_leader_get(int64_t obj){return obj==2||obj==4?1:0;}
static bool dialog_ui_is_in_dialog(int64_t obj){(void)obj;return dialog;}
static bool combat_turn_based_is_active(void){return combat;}
static int64_t combat_input_actor(void){return actor;}
static bool combat_input_ready(int64_t obj){return ready&&obj==actor;}
static bool inven_ui_can_open_inven(int64_t pc,int64_t obj){assert(pc==1&&obj==2);return barter;}
static int obj_field_int32_get(int64_t obj,int field){(void)obj;(void)field;return mind;}
static void inven_ui_open(int64_t obj,int64_t target,int mode){assert(obj==actor&&target==0&&mode==9);opened++;}
static void follower_ui_dispatch_command(int index){dispatched=index;}
'''+function('modern_follower_command_available')+function('modern_follower_command')+r'''
int main(void){
 for(int cmd=0;cmd<8;cmd++)assert(modern_follower_command_available(2,cmd));
 mind=true;assert(!modern_follower_command_available(2,5));mind=false;
 barter=false;assert(!modern_follower_command_available(2,6));barter=true;
 modern_follower_command(2,0);assert(dispatched==0&&follower_ui_subordinate_obj==2&&follower_ui_commander_obj==1);
 combat=true;
 for(actor=2;actor<=4;actor+=2){
  for(int cmd=0;cmd<6;cmd++)assert(!modern_follower_command_available(actor,cmd));
  assert(modern_follower_command_available(actor,6));
  assert(!modern_follower_command_available(actor==2?4:2,6));
  modern_follower_command(actor,6);
  ready=false;assert(!modern_follower_command_available(actor,6));ready=true;
 }
 assert(opened==2);
 dispatched=-1;modern_follower_command(2,0);assert(dispatched==-1);
 assert(modern_follower_command_available(2,7));
 for(int cmd=-1;cmd<=8;cmd++){assert(!modern_follower_command_available(3,cmd));assert(!modern_follower_command_available(0,cmd));}
 dialog=true;assert(!modern_follower_command_available(2,7));
 puts("PASS: companion commands retain native restrictions and cannot bypass tactical ownership or animation waits");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-followers-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
