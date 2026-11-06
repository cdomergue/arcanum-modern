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
script = (root/'source/src/game/script.c').read_text()
reaction = script.split('case SAT_SET_REACTION: {',1)[1].split('\n    case ',1)[0]
code = common + r'''
typedef struct {int op_type[3],op_value[3];} Action;
static int current=50;
static int64_t script_get_obj(int type,int value,void* state){(void)type;(void)state;return value;}
static int script_get_value(int type,int value,void* state){(void)state;assert(type==3);return value;}
static int reaction_get(int64_t npc,int64_t pc){assert(npc==7&&pc==9);return current;}
static void reaction_adj(int64_t npc,int64_t pc,int delta){assert(npc==7&&pc==9);current+=delta;}
#define NEXT 1
static int action_run(Action* action,void* state){
''' + reaction + r'''
int main(void){Action action={{1,2,3},{7,9,80}};assert(action_run(&action,0)==NEXT);assert(current==80);
 action.op_value[2]=20;action_run(&action,0);assert(current==20);}
'''

with tempfile.TemporaryDirectory(prefix='ce-regression-') as folder:
    path=Path(folder)/'test'
    path.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
        '-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',
        '-fno-sanitize-recover=all',str(path.with_suffix('.c')),'-o',str(path)],check=True)
    subprocess.run([str(path)],check=True)
    print('PASS: reaction')
