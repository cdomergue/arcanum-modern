#!/usr/bin/env python3
"""Check animation skipping and walk/run preferences for any controlled ally."""
from pathlib import Path
import os, re, subprocess, tempfile

root = Path(__file__).resolve().parents[1]/'source/src/game'
def function(file, name):
    source = (root/file).read_text()
    m = re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    assert m,name
    return source[m.start():source.index('\n}',m.end())+2]+'\n'

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define ALWAYS_RUN_KEY 1
#define SDL_KMOD_CTRL 1
#define SDL_KMOD_NUM 2
#define MULTIPLAYER_ALWAYS_RUN 1
#define AG_ATTACK 1
#define OBJ_F_NPC_FLAGS 1
#define ONF_BACKING_OFF 1
static bool tactical_enabled=true,combat_turn_based_active=true,combat_fast_turn_based;
static bool controlled[5]={false,true,true,false,true},armed,attacking,backing;
static bool always_run,network;
static int64_t actor=2;
static int settings,modifiers;
static bool combat_player_controls(int64_t obj){return controlled[obj]&&(obj==1||(tactical_enabled&&combat_turn_based_active));}
static int64_t combat_turn_based_whos_turn_get(void){return actor;}
static bool player_is_pc_obj(int64_t obj){return obj==1;}
static bool combat_critter_is_combat_mode_active(int64_t obj){(void)obj;return armed;}
static bool anim_is_current_goal_type(int64_t obj,int goal,void* id){(void)obj;(void)goal;(void)id;return attacking;}
static int obj_field_int32_get(int64_t obj,int field){(void)obj;(void)field;return backing;}
static bool tig_net_is_active(void){return network;}
static int multiplayer_find_slot_from_obj(int64_t obj){return obj==1?0:-1;}
static int multiplayer_flags_get(int slot){(void)slot;return 1;}
static int settings_get_value(int* s,int key){(void)s;(void)key;return always_run;}
static bool tig_kb_get_modifier(int flag){return (modifiers&flag)!=0;}
''' + function('combat.c','sub_4B8040') + function('anim.c','get_always_run') + r'''
int main(void){
 /* Both allies animate even before drawing weapons; no Virgil-specific ID. */
 for(actor=2;actor<=4;actor+=2){
  for(int fast=0;fast<=1;fast++){
   combat_fast_turn_based=fast;
   assert(!sub_4B8040(actor));
  }
  for(int run=0;run<=1;run++)for(modifiers=0;modifiers<4;modifiers++){
   always_run=run;assert(get_always_run(actor)==get_always_run(1));
  }
 }
 /* Enemy/forced AI behavior keeps the original animation policy. */
 actor=3;combat_fast_turn_based=false;assert(sub_4B8040(3));
 armed=true;assert(!sub_4B8040(3));armed=false;
 attacking=true;assert(!sub_4B8040(3));attacking=false;
 backing=true;assert(!sub_4B8040(3));backing=false;
 assert(!sub_4B8040(2));
 actor=2;controlled[2]=false;assert(sub_4B8040(2));controlled[2]=true;
 /* Outside tactical combat, NPC preferences ignore the player's modifier. */
 combat_turn_based_active=false;always_run=true;modifiers=SDL_KMOD_CTRL;
 assert(get_always_run(2)&&!get_always_run(1));
 network=true;assert(get_always_run(1)&&!get_always_run(2));
 puts("PASS: multiple allies animate, enemy policy unchanged, player-equivalent walk/run controls, outside-combat and network isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-movement-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
