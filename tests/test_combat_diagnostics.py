"""Run the production combat logger against damage, deletion and I/O failures."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
logger = root / 'source/src/game/modern_combat_log.inc'
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>
enum {OBJ_F_TYPE,OBJ_F_SPELL_FLAGS,OBJ_F_CRITTER_FLAGS,OBJ_F_CRITTER_FLAGS2,OBJ_F_NPC_COMBAT_FOCUS};
#define OBJ_TYPE_NPC 16
#define OSF_SHIELDED 0x400
typedef struct {int slot_num;} AnimID;
typedef struct {int type;} Goal;
typedef struct {int current_goal;Goal goals[1];} AnimRunInfo;
typedef struct {int spell;} MagicTechRunInfo;
typedef struct {int64_t attacker_obj,target_obj,weapon_obj,field_28;unsigned flags,dam_flags;int dam[5];} CombatContext;
static AnimRunInfo run={0,{{18}}};
static MagicTechRunInfo magictech_run_info[]={{30}};
static int hp[3]={0,100,90},calls,behavior;
static bool alive=true;
static bool obj_handle_is_valid(int64_t obj){return obj==1||(obj==2&&alive);}
static bool obj_type_is_critter(int type){return type==16;}
static int obj_field_int32_get(int64_t obj,int field){
 assert(obj_handle_is_valid(obj));
 return field==OBJ_F_TYPE?16:field==OBJ_F_SPELL_FLAGS?OSF_SHIELDED:8;
}
static int64_t obj_field_handle_get(int64_t obj,int field){assert(obj_handle_is_valid(obj));assert(field==OBJ_F_NPC_COMBAT_FOCUS);return 3-obj;}
static int object_hp_current(int64_t obj){assert(obj_handle_is_valid(obj));return hp[obj];}
static int critter_fatigue_current(int64_t obj){assert(obj_handle_is_valid(obj));return 40;}
static bool combat_critter_is_combat_mode_active(int64_t obj){assert(obj_handle_is_valid(obj));return true;}
static bool combat_turn_based_is_active(void){return true;}
static bool sub_423300(int64_t obj,AnimID* id){assert(obj_handle_is_valid(obj));id->slot_num=17;return true;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** out){assert(id->slot_num==17);*out=&run;return true;}
static bool sub_459170(int64_t obj,int flag,int* index){assert(obj_handle_is_valid(obj)&&flag==OSF_SHIELDED);*index=0;return true;}
''' + '#include "' + logger.as_posix() + '"\n' + r'''
static void combat_dmg_impl(CombatContext* combat){
 calls++; assert(errno==ERANGE);
 if(behavior==1)alive=false;
 else if(behavior!=2)hp[combat->target_obj]-=combat->dam[0];
 combat->dam_flags=0x200000;
 errno=EDOM;
}
static void read_log(char* line,size_t size){
 FILE* f=fopen("ArcanumModern/combat-events.log","rb");assert(f);
 assert(fgets(line,size,f));fclose(f);
}
int main(void){
 CombatContext hit={.attacker_obj=1,.target_obj=2,.field_28=2,.flags=0x102,.dam={10,0,0,0,0}};
 errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==1&&hp[2]==80);
 assert(mkdir("ArcanumModern",0700)==0);
 errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==2&&hp[2]==70);
 char line[4096];read_log(line,sizeof(line));
 assert(line[4]=='-'&&line[10]=='T');
 assert(strstr(line,"event=damage tb=1 flags=102 damage_flags=200000"));
 assert(strstr(line,"before_t={obj=2 valid=1 type=16 hp=80"));
 assert(strstr(line," t={obj=2 valid=1 type=16 hp=70"));
 assert(strstr(line,"slot=17 goal=18 spells=400 shield=30"));
 assert(strstr(line,"input_damage=10,0,0,0,0"));
 assert(unlink("ArcanumModern/combat-events.log")==0);
 behavior=1;errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==3);
 read_log(line,sizeof(line));assert(strstr(line," t={obj=2 valid=0 type=-1"));
 assert(unlink("ArcanumModern/combat-events.log")==0);
 behavior=2;hit.target_obj=0;errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==4);
 read_log(line,sizeof(line));assert(strstr(line," t={obj=0 valid=0 type=-1"));
 FILE* f=fopen("ArcanumModern/combat-events.log","ab");assert(f);
 for(int i=0;i<1024*1024;i++){fputc('x',f);}fclose(f);
 errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==5);
 struct stat st;assert(stat("ArcanumModern/combat-events.previous.log",&st)==0&&st.st_size>=1024*1024);
 assert(stat("ArcanumModern/combat-events.log",&st)==0&&st.st_size<4096);
 // A directory occupying the destination makes rotation fail. Gameplay still runs.
 assert(unlink("ArcanumModern/combat-events.previous.log")==0);
 assert(mkdir("ArcanumModern/combat-events.previous.log",0700)==0);
 f=fopen("ArcanumModern/combat-events.previous.log/keep","wb");assert(f);fclose(f);
 f=fopen("ArcanumModern/combat-events.log","ab");assert(f);
 for(int i=0;i<1024*1024;i++){fputc('x',f);}fclose(f);
 errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==6);
 assert(stat("ArcanumModern/combat-events.log",&st)==0);long length=st.st_size;
 errno=ERANGE;combat_dmg(&hit);assert(errno==EDOM&&calls==7);
 assert(stat("ArcanumModern/combat-events.log",&st)==0&&st.st_size==length);
 puts("PASS: combat snapshots, HP deltas, vanished/null actors, rotation, failed rotation, missing directory, errno and damage delegation");
}
'''
with tempfile.TemporaryDirectory(prefix='combat-diagnostics-') as folder:
    binary = Path(folder) / 'test'
    binary.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-fsanitize=undefined', '-fno-sanitize-recover=all',
                    str(binary.with_suffix('.c')), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], cwd=folder, check=True)

combat = (root / 'source/src/game/combat.c').read_text()
critical = combat[combat.index('void combat_process_crit_miss(CombatContext* combat)\n{'):]
assert critical.index('combat_diagnostic_event("critical_miss"') < critical.index('combat->target_obj = combat->attacker_obj;')
assert 'combat_diagnostic_event("body_of_fire", &body_of_fire' in combat
assert 'static void combat_dmg_impl(CombatContext* combat)\n{' in combat
