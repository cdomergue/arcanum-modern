#!/usr/bin/env python3
"""Execute production turn ownership, PA spending and animation completion gates.

The real game still owns damage, AP costs and serialization. These tests check
that manual companion control cannot borrow another actor's turn or PC fatigue.
"""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'source/src'
source=(root/'game/combat.c').read_text()
def function(name):
    m=re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    assert m,name
    end=source.index('\n}',m.end())+2
    return source[m.start():end]+'\n'
names=['combat_player_controls','combat_input_actor','combat_input_ready',
       'combat_input_end_turn','combat_check_action_points','combat_consume_action_points',
       'combat_turn_based_end_critter_turn','sub_4B7080','combat_tb_timeevent_process']
prelude=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_TYPE_NPC 1
#define OBJ_F_TYPE 1
#define OBJ_F_CRITTER_FLAGS 2
#define OBJ_F_SPELL_FLAGS 3
#define OBJ_F_LOCATION 4
#define OCF_FLEEING 1
#define OCF_SPELL_FLEE 2
#define OCF_SURRENDERED 4
#define OCF_PARALYZED 8
#define OCF_STUNNED 16
#define OSF_MIND_CONTROLLED 32
#define OSF_STONED 64
#define AG_ANIM_FIDGET 1
#define DAMAGE_TYPE_FATIGUE 0
#define TIMEEVENT_TYPE_TB_COMBAT 1
typedef struct {int value;} AnimID;
typedef struct {int obj;} ObjectNode;
typedef struct {int flags,dam[1];} CombatContext;
typedef struct {int type;} TimeEvent;
typedef struct {int delay;} DateTime;
static ObjectNode turn={2};static ObjectNode* dword_5FC240=&turn;
static bool tactical_enabled=true,combat_turn_based_active=true;
static bool camera_blocked;
static bool modern_combat_input_blocked(void){return camera_blocked;}
static int combat_action_points=8,combat_required_action_points;
static int64_t qword_5FC258;
static int dword_5FC260,dword_5FC264;
static int flags[5],spells[5],leader[5]={0,0,1,0,1},controller[5],fatigue[5]={0,30,30,30,30};
static bool busy[5],fidget[5],active[5]={false,true,true,true,true};
static int advanced,scheduled,damage,callback_value,rejections;
static void modern_combat_rejected(int64_t actor,int64_t target,int reason,int cost){assert(actor==2&&target==0&&reason==0&&cost==4&&combat_action_points==1);rejections++;}
static void ap_callback(int value){callback_value=value;}
static struct {void(*field_C)(int);} combat_callbacks={ap_callback};
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static int64_t player_get_local_pc_obj(void){return 1;}
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<5;}
static int obj_field_int32_get(int64_t obj,int field){assert(obj_handle_is_valid(obj));return field==OBJ_F_TYPE?OBJ_TYPE_NPC:field==OBJ_F_CRITTER_FLAGS?flags[obj]:spells[obj];}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)field;return obj;}
static int64_t critter_pc_leader_get(int64_t obj){return obj?leader[obj]:0;}
static bool sub_459040(int64_t obj,int flag,int64_t* out){(void)flag;*out=controller[obj];return *out!=0;}
static int64_t combat_turn_based_whos_turn_get(void){return dword_5FC240?dword_5FC240->obj:0;}
static bool critter_is_active(int64_t obj){return active[obj];}
static bool sub_423300(int64_t obj,AnimID* id){if(id)id->value=1;return busy[obj];}
static bool anim_is_current_goal_type(int64_t obj,int goal,AnimID* id){(void)goal;id->value=1;return fidget[obj];}
static bool anim_id_is_equal(AnimID* a,AnimID* b){return a->value==b->value;}
static int critter_fatigue_current(int64_t obj){return fatigue[obj];}
static void combat_turn_based_next_subturn(void){advanced++;}
static void sub_4B2210(int64_t a,int64_t b,CombatContext* c){(void)a;(void)b;c->flags=0;}
static void combat_dmg(CombatContext* c){damage+=c->dam[0];}
static void combat_debug(int64_t obj,const char* msg){(void)obj;(void)msg;}
static bool gamelib_in_reset(void){return false;}
static bool sub_45C0E0(int type){(void)type;return false;}
static void sub_45A950(DateTime* d,int delay){d->delay=delay;}
static void timeevent_add_delay(TimeEvent* e,DateTime* d){assert(e->type==TIMEEVENT_TYPE_TB_COMBAT&&d->delay==2);scheduled++;}
static int combat_action_points_get(void){return combat_action_points;}
static const char* SDL_getenv(const char* key){(void)key;return NULL;}
#define tig_debug_printf(...) ((void)0)
bool combat_consume_action_points(int64_t obj,int points);
void combat_turn_based_end_critter_turn(int64_t obj);
'''
checks=r'''
int main(void){
 assert(combat_input_actor()==2&&player_get_local_pc_obj()==1);
 assert(combat_input_ready(2)&&!combat_input_ready(1)&&!combat_input_ready(3));
 camera_blocked=true;
 assert(!combat_input_ready(2));
 assert(!combat_input_end_turn(2)&&advanced==0&&combat_action_points==8);
 camera_blocked=false;
 int before=combat_action_points;
 for(int i=0;i<100;i++)assert(combat_input_actor()==2);
 assert(combat_action_points==before); /* inspecting/selecting never refills PA */
 busy[2]=true;assert(!combat_input_ready(2));
 assert(!combat_input_end_turn(2)&&advanced==0&&combat_action_points==8);
 fidget[2]=true;assert(combat_input_ready(2));busy[2]=fidget[2]=false;
 assert(!combat_input_end_turn(1)&&advanced==0);
 flags[2]=OCF_STUNNED;assert(!combat_input_ready(2));flags[2]=0;
 flags[2]=OCF_FLEEING;assert(!combat_player_controls(2));flags[2]=0;
 spells[2]=OSF_MIND_CONTROLLED;controller[2]=3;assert(!combat_player_controls(2));
 controller[2]=1;assert(combat_player_controls(2));spells[2]=0;
 leader[2]=0;assert(combat_input_actor()==0);leader[2]=1;
 assert(combat_check_action_points(2,4));
 assert(combat_consume_action_points(2,4)&&combat_action_points==4&&damage==0);
 assert(!combat_check_action_points(2,6)&&rejections==0); /* no PC-only overexertion for NPCs */
 sub_4B7080();assert(scheduled==0);
 TimeEvent event={0};combat_tb_timeevent_process(&event);assert(advanced==0);
 assert(combat_input_end_turn(2)&&advanced==1&&combat_action_points==0);
 advanced=0;combat_action_points=4;turn.obj=3;
 assert(combat_input_actor()==0&&!combat_input_end_turn(3));
 sub_4B7080();assert(scheduled==1); /* enemy completion still drives its AI turn */
 combat_action_points=0;combat_tb_timeevent_process(&event);assert(advanced==1);
 turn.obj=1;combat_action_points=1;
 assert(combat_check_action_points(1,4));
 assert(combat_consume_action_points(1,4)&&damage==2&&combat_action_points==0);
 turn.obj=2;combat_action_points=1;damage=0;advanced=0;
 assert(!combat_consume_action_points(2,4)&&damage==0&&advanced==1&&rejections==1);
 /* Incapacitation after an action must release the manual turn. */
 turn.obj=2;combat_action_points=4;advanced=scheduled=0;active[2]=false;
 sub_4B7080();assert(combat_action_points==0&&scheduled==1);
 combat_tb_timeevent_process(&event);assert(advanced==1);
 combat_action_points=4;advanced=0;
 combat_tb_timeevent_process(&event);assert(advanced==1&&combat_action_points==0);
 active[2]=true;
 combat_turn_based_active=false;assert(combat_input_actor()==1);
 assert(combat_consume_action_points(2,4)&&damage==0);
 puts("PASS: actor ownership, stale/end-turn input, animation wait, fear/control, original PA and fatigue rules");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-tactical-test-') as directory:
 p=Path(directory);(p/'test.c').write_text(prelude+''.join(function(n) for n in names)+checks)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-function',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# Read-only boundary: no execution APIs may enter this code path.
preview=(root/'ui/intgame.c').read_text().split('bool modern_combat_preview(')[1]
for forbidden in ['combat_consume_action_points(', 'combat_check_action_points(', 'random_between(', 'object_script_execute(', 'magictech_invocation_run(']:
 assert forbidden not in preview,forbidden
print('PASS: preview has no combat execution, script or RNG calls')
