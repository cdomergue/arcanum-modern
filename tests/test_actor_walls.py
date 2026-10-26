#!/usr/bin/env python3
"""Check wall/roof observer selection without changing player identity."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/src/game/wallcheck.c').read_text()
def function(name):
    return re.search(r'^(?:static )?(?:void|int64_t) ' + name + r'\([^\n]*\)\n\{.*?^\}', source, re.M | re.S).group()

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_LOCATION 1
typedef int tig_timestamp_t;
static int64_t wallcheck_observer,qword_5E0A08,pc=1,actor=1,selected;
static bool wallcheck_dirty,tactical=true,combat;
static bool active[5]={0,true,true,true,true};
static int64_t locations[5]={0,100,200,300,400},wall_loc,roof_loc;
static int recalcs;
static bool combat_tactical_enabled(void){return tactical;}
static bool combat_turn_based_is_active(void){return combat;}
static int64_t combat_turn_based_whos_turn_get(void){return actor;}
static int64_t player_get_local_pc_obj(void){return pc;}
static int64_t combat_input_actor(void){return selected?selected:pc;}
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<5;}
static bool critter_is_active(int64_t obj){assert(obj_handle_is_valid(obj));return active[obj];}
static int64_t obj_field_int64_get(int64_t obj,int field){assert(obj_handle_is_valid(obj)&&field==OBJ_F_LOCATION);return locations[obj];}
static void roof_recalc(int64_t loc){roof_loc=loc;}
static void wallcheck_recalc(int64_t loc){wall_loc=loc;recalcs++;}
''' + '\n'.join(function(n) for n in ['wallcheck_observer_get', 'wallcheck_ping', 'sub_438530']) + r'''
static void check(int64_t expected){
 wallcheck_ping(0);assert(wallcheck_observer==expected&&wall_loc==locations[expected]&&roof_loc==wall_loc);
 assert(pc==1); /* Quests and scripts still see the actual player character. */
}
int main(void){
 check(1);int count=recalcs;check(1);assert(recalcs==count);
 combat=true;actor=2;check(2);assert(recalcs==count+1);
 locations[2]++;check(2);assert(recalcs==count+2); /* Movement without a PC hook. */
 locations[1]++;sub_438530(1);assert(!wallcheck_dirty);check(2);
 sub_438530(2);assert(wallcheck_dirty);check(2);
 actor=3;check(3);actor=4;check(4); /* Follow enemies as well as allies. */
 locations[4]++;check(4);sub_438530(4);assert(wallcheck_dirty);check(4);
 active[4]=false;check(1);active[4]=true;
 actor=2;check(2);active[2]=false;check(1);active[2]=true;
 actor=99;check(1);actor=0;check(1);
 actor=2;check(2);combat=false;check(1);
 selected=3;check(3);locations[3]++;check(3); /* Manual companion control outside combat. */
 selected=2;check(2);selected=0;check(1);
 combat=true;tactical=false;check(1);
 tactical=true;actor=2;locations[2]=locations[1];count=recalcs;check(2);
 assert(recalcs==count+1); /* Selection change even on the same tile. */
 combat=false;pc=0;wallcheck_ping(0); /* Main menu/reset without an actor. */
 puts("PASS: stationary turn selection, ally/enemy movement, out-of-combat companion selection, invalid/dead actors, classic mode and combat exit; player identity unchanged");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-companion-walls-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
