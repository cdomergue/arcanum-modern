"""Behavioral regressions for CE #128 and PRs #162/#163, using production C."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def function(file, signature):
    text = (root / 'source' / file).read_text()
    start = text.index(signature + '\n{')
    end = text.index('\n}', start) + 2
    return text[start:end]

common = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ASSERT assert
#define OBJ_HANDLE_NULL 0
enum { OBJ_F_TYPE, OBJ_F_CRITTER_FLAGS, OBJ_F_CRITTER_FLAGS2, OBJ_F_CURRENT_AID,
 OBJ_F_NPC_FLAGS, OBJ_F_NPC_WAIT, OBJ_F_SPELL_FLAGS, OBJ_TYPE_PC=20, OBJ_TYPE_NPC,
 STAT_RACE, RACE_DWARF, STAT_CHARISMA, BASIC_SKILL_PERSUATION, TRAINING_NONE=0 };
#define OCF_STUNNED 1
#define OCF_FATIGUE_LIMITING 2
#define OCF2_FATIGUE_DRAINING 4
#define ONF_AI_WAIT_HERE 8
#define OSF_MIND_CONTROLLED 16
static int fields[16];
static int obj_field_int32_get(int64_t obj,int field){(void)obj;return fields[field];}
'''
code = common + r'''
enum {PRIORITY_3=3,TIMEEVENT_TYPE_NPC_WAIT_HERE};
typedef struct {int days,milliseconds;} DateTime;
typedef struct {int type;union {int64_t object_value;} params[3];} TimeEvent;
static DateTime stru_5B5088[6]={{0,0},{0,900000},{0,7200000},{1,0},{7,0},{30,0}};
static bool familiar,extraordinary;static int training,events,disbands;static TimeEvent last;static DateTime delay;
static bool critter_is_bound_familiar(int64_t obj){(void)obj;return familiar;}
static void sub_424070(int64_t obj,int priority,bool a,bool b){(void)obj;(void)priority;(void)a;(void)b;}
static void obj_field_int32_set(int64_t obj,int field,int value){(void)obj;fields[field]=value;}
static int64_t critter_leader_get(int64_t obj){(void)obj;return 9;}
static bool stat_is_extraordinary(int64_t obj,int stat){assert(obj==9);(void)stat;return extraordinary;}
static int basic_skill_training_get(int64_t obj,int skill){(void)obj;(void)skill;return training;}
static void timeevent_add_delay(TimeEvent* event,DateTime* dt){events++;last=*event;delay=*dt;}
static void critter_disband(int64_t obj,bool b){(void)obj;(void)b;disbands++;}
static void critter_leader_set(int64_t obj,int64_t leader){(void)obj;assert(leader==9);}
''' + function('src/game/ai.c','void ai_npc_wait(int64_t obj)') + r'''
static void reset(void){memset(fields,0,sizeof(fields));fields[OBJ_F_TYPE]=OBJ_TYPE_NPC;fields[OBJ_F_NPC_WAIT]=1;events=disbands=0;}
int main(void){
 reset();ai_npc_wait(7);assert(events==1&&last.params[0].object_value==7&&delay.milliseconds==900000&&disbands==1);
 ai_npc_wait(7);assert(events==1);
 reset();extraordinary=true;ai_npc_wait(7);assert(events==0&&disbands==1);
 reset();extraordinary=false;fields[OBJ_F_SPELL_FLAGS]=OSF_MIND_CONTROLLED;ai_npc_wait(7);assert(events==0);
 reset();training=1;ai_npc_wait(7);assert(events==1&&delay.milliseconds==7200000);
 reset();fields[OBJ_F_NPC_WAIT]=5;ai_npc_wait(7);assert(events==0);
 reset();familiar=true;ai_npc_wait(7);assert(events==0&&disbands==0&&(fields[OBJ_F_NPC_FLAGS]&ONF_AI_WAIT_HERE));
 reset();familiar=false;fields[OBJ_F_TYPE]=OBJ_TYPE_PC;ai_npc_wait(7);assert(events==0&&disbands==0);
}
'''

with tempfile.TemporaryDirectory(prefix='ce-regression-') as folder:
    path=Path(folder)/'test'
    path.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
        '-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',
        '-fno-sanitize-recover=all',str(path.with_suffix('.c')),'-o',str(path)],check=True)
    subprocess.run([str(path)],check=True)
    print('PASS: waiting')
