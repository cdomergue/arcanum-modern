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
enum { AGDATA_SCRATCH_VAL5, AG_ANIMATE_STUNNED, PRIORITY_HIGHEST };
typedef int AnimID;
typedef struct { int data; } Param;
typedef struct { Param params[1]; } Stack;
typedef struct { Stack* cur_stack_data; } AnimRunInfo;
static Stack stack; static AnimRunInfo run={&stack};
static bool turn_based=true,goal=true,valid=true; static int interruptions;
static bool combat_turn_based_is_active(void){return turn_based;}
static int64_t combat_turn_based_whos_turn_get(void){return 99;}
static bool anim_is_current_goal_type(int64_t obj,int type,AnimID* id){assert(obj==7);(void)type;*id=1;return goal;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** out){(void)id;*out=&run;return valid;}
static void anim_interrupt(AnimID* id,int priority){(void)id;(void)priority;interruptions++;}
''' + function('src/game/anim.c','bool sub_427000(int64_t obj)') + r'''
int main(void){
 fields[OBJ_F_CRITTER_FLAGS]=OCF_STUNNED;stack.params[0].data=6;
 sub_427000(7);assert(stack.params[0].data==3&&interruptions==0);
 sub_427000(7);assert(stack.params[0].data==0&&interruptions==1);
 stack.params[0].data=6;turn_based=false;sub_427000(7);assert(stack.params[0].data==6);
 turn_based=true;goal=false;sub_427000(7);assert(stack.params[0].data==6);
 goal=true;valid=false;sub_427000(7);assert(stack.params[0].data==6);
 valid=true;fields[OBJ_F_CRITTER_FLAGS]=0;sub_427000(7);assert(stack.params[0].data==6);
}
'''

with tempfile.TemporaryDirectory(prefix='ce-regression-') as folder:
    path=Path(folder)/'test'
    path.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
        '-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',
        '-fno-sanitize-recover=all',str(path.with_suffix('.c')),'-o',str(path)],check=True)
    subprocess.run([str(path)],check=True)
    print('PASS: stun')
