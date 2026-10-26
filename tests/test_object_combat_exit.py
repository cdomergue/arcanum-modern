#!/usr/bin/env python3
"""Cancel party object attacks without clearing critter hostility or other goals."""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/src/game/combat.c').read_text()
start = source.index('static bool combat_is_object_target(')
end = source.index('// 0x4B3F20', start)
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_TYPE 1
#define OBJ_F_NPC_COMBAT_FOCUS 2
#define OBJ_TYPE_NPC 2
#define AG_ATTEMPT_ATTACK 7
#define AGDATA_TARGET_OBJ 0
#define PRIORITY_5 5
typedef struct {int actor;} AnimID;
typedef struct {struct {struct {int64_t obj;} params[1];} goals[1];} AnimRunInfo;
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct {ObjectNode* head;} ObjectList;
static struct {int type;int64_t focus;bool locked,fighting,armed,attack,other_goal;AnimRunInfo run;} actors[10];
static ObjectNode second={3,NULL},first={2,&second};
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<10;}
static int obj_field_int32_get(int64_t obj,int field){assert(field==OBJ_F_TYPE);return actors[obj].type;}
static bool obj_type_is_critter(int type){return type==1||type==2;}
static int64_t obj_field_handle_get(int64_t obj,int field){assert(field==OBJ_F_NPC_COMBAT_FOCUS);return actors[obj].focus;}
static bool anim_is_current_goal_type(int64_t obj,int goal,AnimID* id){assert(goal==AG_ATTEMPT_ATTACK);id->actor=obj;return actors[obj].attack;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** run){*run=&actors[id->actor].run;return true;}
static bool anim_interrupt(AnimID* id,int priority){assert(priority==PRIORITY_5);actors[id->actor].attack=false;return true;}
static void ai_target_unlock(int64_t obj){actors[obj].locked=false;}
static void sub_4AA300(int64_t obj,int64_t target){assert(actors[obj].focus==target);actors[obj].focus=0;actors[obj].fighting=false;}
static bool ai_is_fighting(int64_t obj){return actors[obj].fighting;}
static void combat_critter_deactivate_combat_mode(int64_t obj){actors[obj].armed=false;}
static void object_list_all_followers(int64_t pc,ObjectList* list){assert(pc==1);list->head=&first;}
static void object_list_destroy(ObjectList* list){list->head=NULL;}
''' + source[start:end] + r'''
static void reset(void){
 memset(actors,0,sizeof(actors));actors[1].type=1;
 for(int i=2;i<=5;i++)actors[i].type=OBJ_TYPE_NPC;
 actors[6].type=3; /* Destructible object. */
}
static void attack(int actor,int target,bool focus){
 actors[actor].armed=true;actors[actor].attack=true;
 actors[actor].run.goals[0].params[0].obj=target;
 if(focus){actors[actor].focus=target;actors[actor].fighting=true;actors[actor].locked=true;}
}
int main(void){
 reset();attack(1,6,false);attack(2,6,true);attack(3,6,false);attack(4,6,true);
 combat_stop_party_object_attacks(1);
 assert(!actors[1].attack&&!actors[2].attack&&!actors[3].attack);
 assert(!actors[2].focus&&!actors[2].locked&&!actors[2].fighting&&!actors[2].armed&&!actors[3].armed);
 assert(actors[4].attack&&actors[4].focus==6&&actors[4].armed); /* Unrelated NPC. */
 reset();attack(2,5,true);actors[3].other_goal=true;
 combat_stop_party_object_attacks(1);
 assert(actors[2].attack&&actors[2].focus==5&&actors[2].fighting&&actors[2].locked&&actors[2].armed);
 assert(actors[3].other_goal); /* Preserve an unrelated move/spell. */
 reset();attack(2,6,true);actors[2].attack=false;
 combat_stop_party_object_attacks(1);assert(!actors[2].focus&&!actors[2].fighting&&!actors[2].armed);
 reset();attack(2,6,false);actors[2].focus=5;actors[2].fighting=true;
 combat_stop_party_object_attacks(1);assert(!actors[2].attack&&actors[2].focus==5&&actors[2].fighting&&actors[2].armed);
 assert(!combat_is_object_target(0)&&!combat_is_object_target(99));
 puts("PASS: party object goals/focus cancelled; pending-only and focus-only attacks; enemy hostility, unrelated NPCs and nonattack goals preserved");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-object-exit-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
