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
enum {MAGICTECH_RUN_FREE=1,MAGICTECH_ACTION_BEGIN=2};
typedef struct {int flags;struct {int64_t obj;} source_obj;int action,spell;} MagicTechRunInfo;
typedef struct {int cost;} Cost;
static Cost magictech_spells[4]={{0},{7}},maintenance[4]={{0},{3}};
static MagicTechRunInfo run={.source_obj={7},.spell=1,.action=MAGICTECH_ACTION_BEGIN};
static bool found=true,magictech_cheat_mode;static int charged=-1,race;
static bool magictech_id_to_run_info(int id,MagicTechRunInfo** out){assert(id==1000);*out=&run;return found;}
static void tig_debug_printf(const char* text){(void)text;}
static Cost* magictech_get_maintenance(int id){assert(id==1);return &maintenance[id];}
static int stat_level_get(int64_t obj,int stat){(void)obj;(void)stat;return race;}
static bool sub_450420(int64_t obj,int cost,int unused,int spell){assert(obj==7&&unused==0&&spell==1);charged=cost;return true;}
''' + function('src/game/magictech.c','bool sub_450940(int mt_id)') + r'''
int main(void){
 fields[OBJ_F_TYPE]=OBJ_TYPE_PC;assert(sub_450940(1000)&&charged==7);
 run.action=0;assert(sub_450940(1000)&&charged==3);
 race=RACE_DWARF;assert(sub_450940(1000)&&charged==6);
 magictech_cheat_mode=true;assert(sub_450940(1000)&&charged==2);
 charged=-1;run.flags=MAGICTECH_RUN_FREE;assert(sub_450940(1000)&&charged==-1);
 run.flags=0;run.source_obj.obj=0;assert(sub_450940(1000)&&charged==-1);
 found=false;assert(!sub_450940(1000));
}
'''

with tempfile.TemporaryDirectory(prefix='ce-regression-') as folder:
    path=Path(folder)/'test'
    path.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
        '-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',
        '-fno-sanitize-recover=all',str(path.with_suffix('.c')),'-o',str(path)],check=True)
    subprocess.run([str(path)],check=True)
    print('PASS: spell_cost')
