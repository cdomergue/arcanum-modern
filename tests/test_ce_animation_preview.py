"""Production animation eligibility stays read-only and matches interruption rules."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]

def function(path, signature):
    text = (root / 'source/src' / path).read_text()
    start = text.index(signature + '\n{')
    return text[start:text.index('\n}', start) + 2]

query = function('game/anim_private.c', 'bool anim_can_interrupt(AnimID* anim_id, int priority)')
actual = function('game/anim_private.c', 'bool anim_interrupt(AnimID* anim_id, int priority)')
# Execute the real interruption eligibility, stopping before its first mutation.
actual = actual[:actual.index('    run_info->flags |= 0x02;')] + '    return true;\n}'
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ASSERT assert
#define PRIORITY_HIGHEST 6
#define PRIORITY_5 5
#define PRIORITY_3 3
#define PRIORITY_2 2
typedef struct {int slot_num;} AnimID;
typedef struct {int type;} Goal;
typedef struct {unsigned int flags;int current_goal;Goal goals[2];} AnimRunInfo;
typedef struct {int field_8,priority_level;} AnimGoalNode;
static AnimRunInfo state;
static AnimGoalNode node;
static AnimGoalNode* anim_goal_nodes[]={&node};
static bool valid,reset;
static bool gamelib_in_reset(void){return reset;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** out){(void)id;*out=&state;return valid;}
''' + query + '\n' + actual + r'''
int main(void){
 AnimID id={0};int cases=0;
 for(int v=0;v<2;v++)for(int active=0;active<2;active++)for(int r=0;r<2;r++)
 for(int current=-1;current<2;current++)for(int eye=0;eye<2;eye++)
 for(int gp=0;gp<=6;gp++)for(int requested=0;requested<=6;requested++){
  valid=v;reset=r;state.flags=active;state.current_goal=current;
  node.field_8=eye;node.priority_level=gp;
  AnimRunInfo before=state;
  assert(anim_can_interrupt(&id,requested)==anim_interrupt(&id,requested));
  assert(memcmp(&state,&before,sizeof(state))==0);cases++;
 }
 printf("PASS: %d native interruption eligibility cases without mutation\n",cases);
}
'''

with tempfile.TemporaryDirectory(prefix='ce-animation-') as folder:
    binary=Path(folder)/'test'
    binary.with_suffix('.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
                    '-Wno-unused-variable','-fsanitize=undefined',
                    '-fno-sanitize-recover=all',str(binary.with_suffix('.c')),
                    '-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)

# Both menu implementations must query; execution keeps its original semantics.
follower=(root/'source/src/ui/follower_ui.c').read_text()
assert 'enabled = inven_ui_can_open_inven_preview(' in follower
assert 'return inven_ui_can_open_inven_preview(pc, obj);' in follower
assert 'if (inven_ui_can_open_inven(follower_ui_commander_obj, follower_ui_subordinate_obj))' in follower

logger=function('ui/follower_ui.c', 'void modern_follower_log_menu(int64_t obj)')
log_code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>
enum {OBJ_F_CURRENT_AID, OBJ_F_SPELL_FLAGS, FOLLOWER_UI_COMMAND_INVENTORY};
typedef struct {int slot_num;} AnimID;
typedef struct {int type;} Goal;
typedef struct {int current_goal;Goal goals[1];} AnimRunInfo;
static AnimRunInfo run={0,{{2}}};
static bool obj_handle_is_valid(int64_t obj){return obj==7;}
static bool sub_423300(int64_t obj,AnimID* id){assert(obj==7);id->slot_num=17;return true;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** out){assert(id->slot_num==17);*out=&run;return true;}
static int obj_field_int32_get(int64_t obj,int field){assert(obj==7);return field==OBJ_F_CURRENT_AID?0x123:4;}
static bool modern_follower_command_available(int64_t obj,int cmd){assert(obj==7&&cmd==FOLLOWER_UI_COMMAND_INVENTORY);return true;}
''' + logger + r'''
int main(void){
 errno=EDOM;modern_follower_log_menu(7);assert(errno==EDOM);
 assert(mkdir("ArcanumModern",0700)==0);
 modern_follower_log_menu(7);assert(errno==EDOM);
 FILE* f=fopen("ArcanumModern/follower-events.log","rb");assert(f);
 char line[1024];assert(fgets(line,sizeof(line),f));fclose(f);
 assert(strstr(line,"event=menu_open actor=7 inventory=1 active=1/1 slot=17/17 goal=2/2 aid=00000123/00000123 spell_flags=4"));
 assert(line[4]=='-'&&line[10]=='T');
 f=fopen("ArcanumModern/follower-events.log","ab");assert(f);
 for(int i=0;i<1024*1024;i++)fputc('x',f);fclose(f);
 modern_follower_log_menu(7);
 f=fopen("ArcanumModern/follower-events.previous.log","rb");assert(f);fclose(f);
 f=fopen("ArcanumModern/follower-events.log","rb");assert(f);
 assert(fgets(line,sizeof(line),f));assert(fgetc(f)==EOF);fclose(f);
 puts("PASS: follower diagnostics fields, timestamp, rotation and missing directory");
}
'''
with tempfile.TemporaryDirectory(prefix='ce-follower-log-') as folder:
    binary=Path(folder)/'test'
    binary.with_suffix('.c').write_text(log_code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',
                    str(binary.with_suffix('.c')),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],cwd=folder,check=True)
