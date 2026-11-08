#!/usr/bin/env python3
"""Production approach search and both animation entry points on wall-edge terrain."""
from pathlib import Path
import os, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
# Reuse the native-path harness, without executing its test main.
namespace = {'__file__': str(root/'tests/test_pathfinding.py')}
prefix = (root/'tests/test_pathfinding.py').read_text().split("code+=r'''\nstatic PathCreateInfo query")[0]
exec(compile(prefix, 'pathfinding-harness', 'exec'), namespace)
code = namespace['code']
code = code.replace('static int wall,gap=', 'static int edge_wall,barrier_kind;static int wall,gap=')
code = code.replace('enum {OBJ_F_TYPE,OBJ_F_FLAGS,OBJ_TYPE_PC,OBJ_TYPE_NPC,OF_NO_BLOCK=1};', '''enum {OBJ_F_TYPE,OBJ_F_FLAGS,OBJ_TYPE_PC,OBJ_TYPE_NPC,OBJ_TYPE_PORTAL,OBJ_TYPE_CONTAINER,OBJ_TYPE_SCENERY,
 OBJ_F_LOCATION,OBJ_F_OFFSET_X,OBJ_F_OFFSET_Y,OBJ_F_CURRENT_AID,OBJ_F_SPELL_FLAGS,OF_NO_BLOCK=1,OF_OFF=2,OF_DESTROYED=4,OSF_POLYMORPHED=8};
static int target_flags,target_type=OBJ_TYPE_CONTAINER;
static int64_t actor_loc,target_loc;
static int64_t obj_field_int64_get(int64_t obj,int f){(void)f;return obj==9?target_loc:actor_loc;}
static bool obj_handle_is_valid(int64_t obj){return obj==1||obj==2||obj==9;}
static void obj_field_int32_set(int64_t obj,int field,int value){assert(obj==9&&field==OBJ_F_FLAGS);target_flags=value;}
static void object_flags_set(int64_t obj,int f){assert(obj==9);target_flags|=f;}
static void object_flags_unset(int64_t obj,int f){assert(obj==9);target_flags&=~f;}
''')
code = code.replace('return field==OBJ_F_TYPE?(obj==1?OBJ_TYPE_PC:OBJ_TYPE_NPC):0;', 'return field==OBJ_F_TYPE?(obj==9?target_type:obj==1?OBJ_TYPE_PC:OBJ_TYPE_NPC):field==OBJ_F_FLAGS&&obj==9?target_flags:0;')
code = code.replace('(void)obj;(void)type;int64_t next;', '(void)obj;*type=0;int64_t next;')
code = code.replace('return blocked(next)||', '''bool cross=edge_wall && ((LOCATION_GET_X(loc)<=130 && LOCATION_GET_X(next)>130)||(LOCATION_GET_X(next)<=130 && LOCATION_GET_X(loc)>130))
 && (gap<0 || LOCATION_GET_Y(loc)!=gap || LOCATION_GET_Y(next)!=gap);
 if(cross){
  if(barrier_kind==2 && !(f&OBJ_TRAVERSAL_WINDOW_BLOCKS)){if(window)*window=true;return false;}
  *block=50;*type=barrier_kind==1?OBJ_TYPE_PORTAL:0;return true;
 }

 if(next==target_loc && target_type==OBJ_TYPE_PORTAL && !(target_flags&OF_OFF) && (f&OBJ_TRAVERSAL_PORTAL_BLOCKS)){*block=9;*type=OBJ_TYPE_PORTAL;return true;}
 if(next==target_loc && !(target_flags&OF_OFF) && !(f&OBJ_TRAVERSAL_SKIP_OBJECTS) && !(target_flags&OF_NO_BLOCK)){*type=OBJ_TYPE_CONTAINER;*block=9;return true;}
 return blocked(next)||''')
code=code.replace('#define OBJ_TRAVERSAL_IGNORE_CRITTERS 4', '#define OBJ_TRAVERSAL_IGNORE_CRITTERS 4\n#define OBJ_TRAVERSAL_SKIP_OBJECTS 8')
code += r'''
#define OBJ_TYPE_WALL 0
#define ASSERT assert
static int64_t location_dist(int64_t a,int64_t b){int64_t x=llabs(LOCATION_GET_X(a)-LOCATION_GET_X(b)),y=llabs(LOCATION_GET_Y(a)-LOCATION_GET_Y(b));return x>y?x:y;}
typedef struct{int8_t rotations[200];int flags,curr,max,field_CC,maxPathLength,absMaxPathLength;int64_t field_E8,field_F0;}AnimPath;
typedef union{int64_t obj,loc,data;}Param;
enum{AGDATA_TARGET_TILE,AGDATA_RANGE_DATA};
typedef struct{Param params[2];}Goal;
typedef struct{Param params[2];int flags,path_attached_to_stack_index,current_goal;Goal* cur_stack_data;AnimPath path;}AnimRunInfo;
static bool combat_turn_based_active;
static bool combat_turn_based_is_active(void){return combat_turn_based_active;}
static bool combat_player_controls(int64_t obj){return obj==1||obj==2;}
static void modern_combat_rejected(int64_t a,int64_t b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static bool sub_425BF0(PathCreateInfo* p,bool preview){(void)p;(void)preview;return true;}
static int tig_art_id_rotation_get(int art){return art;}
static bool critter_can_open_portals(int64_t obj){(void)obj;return true;}
static bool critter_can_jump_window(int64_t obj){(void)obj;return true;}
'''
a=(root/'source/src/game/anim.c').read_text()
for signature in ['void sub_4257E0(int64_t obj, unsigned int* flags_ptr)','bool sub_425840(int64_t a1, int64_t a2, int64_t a3, int a4, int64_t a5)']:
 start=a.index(signature+'\n{');code+=a[start:a.index('\n}',start)+2]+'\n'
code+=(root/'source/src/game/modern_interaction_path.inc').read_text()
# Compile the exact new dispatch from both production approach nodes, avoiding
# unrelated legacy fallbacks. This also asserts both nodes use the shared path.
for name in ['sub_427990','sub_4280D0']:
 start=a.index('bool '+name+'(AnimRunInfo* run_info)\n{')
 end=a.index('\n}',start)
 body=a[start:end]
 dispatch=body[body.index('    range = run_info->cur_stack_data'):]
 dispatch=dispatch[:dispatch.index('\n    }')+6]
 assert 'anim_move_to_interaction' in dispatch
 code+='static bool '+name+'(AnimRunInfo* run_info){int64_t target_obj=run_info->params[1].obj,range;\n'+dispatch+'\nreturn false;}\n'
combat_source=(root/'source/src/game/combat.c').read_text()
start=combat_source.index('bool combat_check_use_obj(')
code+=r'''
static int required_ap,available_ap=200;static bool running;
static bool get_always_run(int64_t obj){(void)obj;return running;}
static int combat_move_cost(int64_t obj,int64_t loc,bool adjacent){(void)obj;(void)loc;(void)adjacent;assert(0);return 0;}
static bool combat_check_action_points(int64_t obj,int points){(void)obj;required_ap=points;return points<=available_ap;}
'''
code+=combat_source[start:combat_source.index('\n}',start)+2]+'\n'
code+=r'''
static void verify(AnimPath* path){int64_t at=actor_loc;for(int i=0;i<path->max;i++){
 int64_t b;int t;bool window;assert(!object_calc_traversal_cost(1,at,path->rotations[i],0,&b,&t,&window));assert(!b);
 assert(location_in_dir(at,path->rotations[i],&at));}assert(at==path->field_F0);assert(anim_interaction_reachable(1,at,9,anim_interaction_range(9)));}
int main(void){
 actor_loc=location_make(129,100);target_loc=location_make(131,100);edge_wall=1;gap=80;
 AnimPath path;assert(!anim_interaction_reachable(1,location_make(130,100),9,1));
 assert(anim_preview_interaction_path(1,9,&path));assert(path.max>30);verify(&path);assert(target_flags==0);
 combat_turn_based_active=true;assert(combat_check_use_obj(1,9));assert(required_ap==2+2*path.max);
 running=true;assert(combat_check_use_obj(1,9));assert(required_ap==2+path.max);
 available_ap=2;assert(!combat_check_use_obj(1,9));available_ap=200;combat_turn_based_active=false;
 /* Both modes must stream the same route, not stop at the geometrically nearest tile. */
 Goal goal={0};goal.params[AGDATA_RANGE_DATA].data=1;AnimRunInfo run={0};run.params[0].obj=1;run.params[1].obj=9;run.cur_stack_data=&goal;
 assert(sub_427990(&run));assert(run.path.max==8);assert(goal.params[AGDATA_TARGET_TILE].loc==target_loc);int8_t route[8];memcpy(route,run.path.rotations,8);
 memset(&run.path,0,sizeof(run.path));assert(sub_4280D0(&run));assert(!memcmp(route,run.path.rotations,8));
 int total=0;while(!anim_interaction_reachable(1,actor_loc,9,1)){
 assert(sub_427990(&run));assert(run.path.max>0&&run.path.max<=8);
 for(int i=0;i<run.path.max;i++)assert(location_in_dir(actor_loc,run.path.rotations[i],&actor_loc));
 total+=run.path.max;assert(total<=200);}assert(target_flags==0);
 assert(anim_preview_interaction_path(1,9,&path)&&path.max==0);
 actor_loc=location_make(129,100);gap=-1;assert(!anim_preview_interaction_path(1,9,&path));assert(target_flags==0);
 target_flags=OF_NO_BLOCK;gap=80;assert(anim_preview_interaction_path(1,9,&path));assert(target_flags==OF_NO_BLOCK);verify(&path);
 combat_turn_based_active=true;gap=-1;assert(!combat_check_use_obj(1,9));combat_turn_based_active=false;gap=80;
 target_flags=OF_OFF;assert(!anim_preview_interaction_path(1,9,&path));assert(target_flags==OF_OFF);target_flags=0;
 /* Intermediate locked doors cannot be bypassed; a jumpable window must
    be crossed by the route before the object can be reached. */
 actor_loc=location_make(120,100);gap=-1;barrier_kind=1;
 assert(!anim_preview_interaction_path(1,9,&path));assert(!anim_interaction_reachable(1,location_make(130,100),9,1));
 barrier_kind=2;assert(anim_preview_interaction_path(1,9,&path));verify(&path);
 assert(LOCATION_GET_X(path.field_F0)>130);assert(!anim_interaction_reachable(1,location_make(130,100),9,1));
 /* A closed portal that is itself the target remains usable, and its flags
    are restored even when another barrier prevents interaction. */
 target_type=OBJ_TYPE_PORTAL;target_flags=OF_NO_BLOCK;edge_wall=0;actor_loc=location_make(130,100);
 assert(anim_preview_interaction_path(1,9,&path)&&path.max==0);assert(target_flags==OF_NO_BLOCK);
 target_flags=0;target_type=OBJ_TYPE_CONTAINER;barrier_kind=0;edge_wall=1;gap=80;actor_loc=location_make(129,100);
 /* Two-tile interactions must check the second edge, too. */
 target_type=OBJ_TYPE_SCENERY;assert(!anim_interaction_reachable(1,actor_loc,9,2));assert(anim_preview_interaction_path(1,9,&path));verify(&path);
 target_loc=location_make(131,100);actor_loc=location_make(129,100);edge_wall=0;assert(anim_preview_interaction_path(1,9,&path)&&path.max==0);
 FILE* f=fopen("ArcanumModern/interaction-path.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);clock_ms+=2000;
 for(int i=0;i<100;i++){errno=EDOM;anim_interaction_log(1,9,1,8,40,"found",3);assert(errno==EDOM);}
 puts("PASS: wall-edge interaction detours, shared exploration/combat approach, streamed arrival, blocked targets, flag restoration, two-tile reach, diagnostic rotation");
}
'''
# Geometry must be validated before consuming the use action's AP.
use=a[a.index('bool sub_428A10(AnimRunInfo* run_info)\n{'):]
assert use.index('if (blocked)') < use.index('combat_consume_action_points(source_obj, 2)')
combat=(root/'source/src/game/combat.c').read_text()
check=combat[combat.index('bool combat_check_use_obj('):combat.index('// 0x4B7A20')]
assert 'anim_preview_interaction_path' in check and '2 + multiplier * path.max' in check
with tempfile.TemporaryDirectory(prefix='arcanum-interaction-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-function','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/interaction-path.previous.log').stat().st_size==1024*1024
 log=(p/'ArcanumModern/interaction-path.log').read_text();assert len(log.splitlines())==20 and 'outcome=found' in log
