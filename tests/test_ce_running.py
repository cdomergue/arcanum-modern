"""Running must not consume fatigue; preserve movement and exhaustion handling."""
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
enum {AGDATA_SCRATCH_VAL4,PRIORITY_HIGHEST,TIG_ART_ANIM_WALK};
typedef int tig_art_id_t; typedef int AnimID;
typedef struct {int64_t obj;int data;} Param;
typedef struct {Param params[1];} Stack;
typedef struct {Param params[1];int current_goal,flags;Stack* cur_stack_data;AnimID id;} AnimRunInfo;
static int damage,energy=30,ap,interruptions,walks,dword_5DE6E4=1;
static bool combat_mode,can_act=true,survives=true;
static int critter_fatigue_current(int64_t obj){(void)obj;return energy;}
static bool combat_critter_is_combat_mode_active(int64_t obj){(void)obj;return combat_mode;}
static int critter_fatigue_damage_get(int64_t obj){(void)obj;return damage;}
static int critter_fatigue_damage_set(int64_t obj,int value){(void)obj;damage=value;return survives;}
static int tig_art_id_anim_set(int art,int anim){(void)art;(void)anim;return 42;}
static void object_set_current_aid(int64_t obj,int art){(void)obj;assert(art==42);walks++;}
static bool combat_consume_action_points(int64_t obj,int value){(void)obj;ap=value;return can_act;}
static void anim_interrupt(AnimID* id,int priority){(void)id;(void)priority;interruptions++;}
''' + function('src/game/anim.c','int sub_430FC0(AnimRunInfo* run_info)') + r'''
int main(void){
 Stack stack={0};AnimRunInfo run={.params={{.obj=7}},.current_goal=1,.flags=0x40,.cur_stack_data=&stack};
 for(int mode=0;mode<2;mode++){
  combat_mode=mode;damage=0;stack.params[0].data=0;
  for(int i=0;i<4;i++){assert(sub_430FC0(&run));assert(damage==0&&ap==1);}
  assert(sub_430FC0(&run));assert(damage==0);
 }
 fields[OBJ_F_CRITTER_FLAGS]=OCF_FATIGUE_LIMITING;damage=0;stack.params[0].data=0;
 for(int i=0;i<19;i++)sub_430FC0(&run);assert(damage==0);sub_430FC0(&run);assert(damage==0);
 fields[OBJ_F_CRITTER_FLAGS]=0;fields[OBJ_F_CRITTER_FLAGS2]=OCF2_FATIGUE_DRAINING;
 damage=0;sub_430FC0(&run);assert(damage==0);
 run.flags=0;sub_430FC0(&run);assert(damage==0&&ap==2);
 run.flags=0x40;energy=0;sub_430FC0(&run);assert(walks==1&&!(run.flags&0x40)&&ap==2);
 // Sustained running must never drain fatigue, with either fatigue modifier.
 for(int mode=0;mode<2;mode++){
  combat_mode=mode;
  for(int modifier=0;modifier<3;modifier++){
   energy=1;damage=29;run.flags=0x40;stack.params[0].data=0;
   fields[OBJ_F_CRITTER_FLAGS]=modifier==1?OCF_FATIGUE_LIMITING:0;
   fields[OBJ_F_CRITTER_FLAGS2]=modifier==2?OCF2_FATIGUE_DRAINING:0;
   for(int i=0;i<10000;i++){assert(sub_430FC0(&run));assert(damage==29&&ap==1);}
  }
 }
 energy=30;run.flags=0x40;combat_mode=true;survives=false;assert(!sub_430FC0(&run));
 survives=true;can_act=false;assert(!sub_430FC0(&run));assert(interruptions==1&&dword_5DE6E4==0);
}
'''

with tempfile.TemporaryDirectory(prefix='ce-regression-') as folder:
    path=Path(folder)/'test'
    path.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
        '-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',
        '-fno-sanitize-recover=all',str(path.with_suffix('.c')),'-o',str(path)],check=True)
    subprocess.run([str(path)],check=True)
    print('PASS: running')
