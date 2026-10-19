from pathlib import Path
import os
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1]/'source/src/game/combat.c').read_text()
match = re.search(r'^void combat_party_engage\([^\n]+\)\n\{', source, re.M)
assert match
body = source[match.start():source.index('\n}', match.end())+2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_TYPE 1
#define OBJ_TYPE_NPC 2
static bool tactical_enabled=true,combat_turn_based_active,turn_based=true,can_start=true;
static int starts,added;
static int64_t pc=1;
static bool combat_is_turn_based(void){return turn_based;}
static int64_t player_get_local_pc_obj(void){return pc;}
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<10;}
static int obj_field_int32_get(int64_t obj,int field){(void)field;return obj==8?3:OBJ_TYPE_NPC;}
static int64_t critter_pc_leader_get(int64_t obj){return obj==2||obj==3?1:0;}
static bool obj_type_is_critter(int type){return type==OBJ_TYPE_NPC;}
static void combat_critter_activate_combat_mode(int64_t obj){assert(obj==pc);starts++;combat_turn_based_active=can_start;}
static void combat_turn_based_add_critter(int64_t obj){assert(obj==2||obj==3||obj==4);added++;}
''' + body + r'''
int main(void){
 combat_party_engage(4,0);assert(!starts);
 combat_party_engage(4,2);assert(starts==1&&added==2&&combat_turn_based_active);
 combat_party_engage(3,4);assert(starts==1&&added==2);
 combat_turn_based_active=false;
 combat_party_engage(3,4);assert(starts==2&&added==4);
 combat_turn_based_active=false;
 combat_party_engage(2,0);assert(starts==3&&added==5);
 combat_turn_based_active=false;tactical_enabled=false;
 combat_party_engage(2,4);assert(starts==3);
 tactical_enabled=true;turn_based=false;
 combat_party_engage(2,4);assert(starts==3);
 turn_based=true;pc=0;
 combat_party_engage(2,4);assert(starts==3);
 pc=1;can_start=false;
 combat_party_engage(2,4);assert(starts==4&&added==5);
 can_start=true;
 combat_party_engage(2,8);assert(starts==5&&added==6);
 puts("PASS: party combat triggers, unrelated NPCs, inactive modes and failed startup");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-party-combat-') as directory:
    path = Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'), '-std=c17', '-Wall', '-Wextra', str(path/'test.c'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
