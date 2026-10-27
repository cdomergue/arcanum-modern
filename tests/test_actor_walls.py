#!/usr/bin/env python3
"""Exercise observer snapshots and simultaneous wall updates from production code."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/game/wallcheck.c').read_text()
combat_source=(root/'source/src/game/combat.c').read_text()
def function(text,name):
    return re.search(r'^(?:static )?(?:void|int|int64_t) '+name+r'\([^\n]*\)\n\{.*?^\}',text,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_LOCATION 1
#define OBJ_F_FLAGS 2
#define OF_DESTROYED 1
#define OF_OFF 2
#define OF_DONTDRAW 4
#define REALLOC realloc
typedef int tig_timestamp_t;
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
static struct {ObjectNode* head;} combat_critter_list;
static ObjectNode nodes[320];
static int64_t wallcheck_observer,qword_5E0A08,pc=1,actor=1,selected;
static int64_t *wallcheck_actors,*wallcheck_previous_actors,*wallcheck_locations;
static int wallcheck_actor_capacity,wallcheck_observer_count;
static bool wallcheck_dirty,wallcheck_enabled=true,tactical=true,combat_turn_based_active;
static bool dead[320],inactive[320];
static int flags[320],recalcs,wall_count,roof_count,restored;
static int64_t locations[320],walls[320],roofs[320];
static bool combat_tactical_enabled(void){return tactical;}
static bool combat_turn_based_is_active(void){return combat_turn_based_active;}
static int64_t combat_turn_based_whos_turn_get(void){return actor;}
static int64_t player_get_local_pc_obj(void){return pc;}
static int64_t combat_input_actor(void){return selected?selected:pc;}
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<320;}
static bool critter_is_active(int64_t obj){assert(obj_handle_is_valid(obj));return !inactive[obj]&&!dead[obj];}
static bool critter_is_dead(int64_t obj){assert(obj_handle_is_valid(obj));return dead[obj];}
static int obj_field_int32_get(int64_t obj,int field){assert(obj_handle_is_valid(obj)&&field==OBJ_F_FLAGS);return flags[obj];}
static int64_t obj_field_int64_get(int64_t obj,int field){assert(obj_handle_is_valid(obj)&&field==OBJ_F_LOCATION);return locations[obj];}
static void roof_fill_off(int64_t loc){assert(loc>0);restored++;}
static void roof_recalc_many(const int64_t* loc,int count){roof_count=count;if(count)memcpy(roofs,loc,count*sizeof(*loc));}
static void wallcheck_begin(void){wall_count=0;}
static void wallcheck_collect(int64_t loc){walls[wall_count++]=loc;}
static void sub_438830(void){recalcs++;}
'''+function(combat_source,'combat_visibility_actors')+'\n'+'\n'.join(function(source,n) for n in ['wallcheck_observer_get','wallcheck_ping','sub_438530','wallcheck_set_enabled'])+r'''
static void group(int count){
 for(int i=0;i<count;i++){nodes[i].obj=i+1;nodes[i].next=i+1<count?&nodes[i+1]:NULL;}
 combat_critter_list.head=count?nodes:NULL;
}
static void check(int count){
 wallcheck_ping(0);assert(wallcheck_observer_count==count && wall_count==count && roof_count==count);
 for(int i=0;i<count;i++)assert(walls[i]==locations[wallcheck_previous_actors[i]]&&roofs[i]==walls[i]);
 assert(pc==1);
}
int main(void){
 for(int i=1;i<320;i++)locations[i]=1000+i;
 check(1);int before=recalcs;check(1);assert(recalcs==before);
 group(4);combat_turn_based_active=true;check(4);
 for(actor=1;actor<=4;actor++){before=recalcs;check(4);assert(recalcs==before);} /* No turn-dependent fading. */
 actor=1;locations[3]++;before=recalcs;check(4);assert(recalcs==before+1); /* Inactive turn's enemy moves. */
 sub_438530(2);assert(wallcheck_dirty);check(4);
 sub_438530(99);assert(!wallcheck_dirty);
 inactive[3]=true;check(4); /* Stunned/unconscious participants are retained. */
 dead[3]=true;check(3);dead[3]=false;check(4);
 flags[4]=OF_OFF;check(3);flags[4]=OF_DONTDRAW;check(3);flags[4]=0;check(4);
 group(300);check(300);assert(wallcheck_actor_capacity>=300);
 int64_t limited[2];assert(combat_visibility_actors(NULL,0)==300);assert(combat_visibility_actors(limited,2)==2);
 before=restored;group(2);check(2);assert(restored==before+300);
 combat_turn_based_active=false;check(1);assert(wallcheck_observer==pc);
 selected=2;check(1);assert(wallcheck_observer==2);
 locations[2]++;check(1);selected=0;check(1);
 combat_turn_based_active=true;tactical=false;check(1);tactical=true;check(2);
 wallcheck_set_enabled(false);wallcheck_ping(0);assert(!wall_count);
 wallcheck_set_enabled(true);check(2);
 combat_turn_based_active=false;pc=0;wallcheck_ping(0);assert(!wall_count&&!roof_count&&!wallcheck_observer_count);
 free(wallcheck_actors);free(wallcheck_previous_actors);free(wallcheck_locations);
 puts("PASS: simultaneous ally/enemy visibility, inactive-turn movement, incapacitated actors, entry/removal, 300 participants, classic mode and selected companion outside combat");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-all-actor-walls-') as d:
    p=Path(d);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True,env=dict(os.environ,ASAN_OPTIONS='detect_leaks=0'))
