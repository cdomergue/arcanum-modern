#!/usr/bin/env python3
"""Exercise world right-click combat exit with an idle controlled companion."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/intgame.c').read_text()
start=source.index('case TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:',source.index('void intgame_process_event('))
start=source.index('                if (pc_obj != OBJ_HANDLE_NULL)',start)
end=source.index('                        if (sub_424070',start)
right_click=source[start:end]+'                    }\n                }\n'
toggle=re.search(r'^void intgame_combat_mode_toggle\(void\)\n\{.*?^\}',source,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_SPELL_FLAGS 1
#define OBJ_F_CRITTER_FLAGS 2
#define OSF_STONED 1
#define OCF_PARALYZED 2
#define OCF_STUNNED 4
#define AG_ANIM_FIDGET 1
#define UI_MSG_TYPE_FEEDBACK 1
typedef struct {int num;const char* str;} MesFileEntry;
typedef struct {int type;const char* str;} UiMessage;
static bool turn_based,armed[5],under_attack,active=true,prone,busy,fidget,concealed;
static int spell_flags,critter_flags,feedback,activations;
static int intgame_mes_file;
static int64_t qword_64C690;
static int64_t player_get_local_pc_obj(void){return 1;}
static bool critter_is_dead(int64_t obj){(void)obj;return false;}
static bool critter_is_active(int64_t obj){(void)obj;return active;}
static bool critter_is_prone(int64_t obj){(void)obj;return prone;}
static bool combat_turn_based_is_active(void){return turn_based;}
static bool combat_critter_is_combat_mode_active(int64_t obj){return armed[obj];}
static bool combat_can_exit_combat_mode(int64_t obj){assert(obj==1);return !under_attack;}
static void combat_critter_deactivate_combat_mode(int64_t obj){assert(obj==1);armed[1]=false;turn_based=false;}
static void combat_critter_activate_combat_mode(int64_t obj){assert(obj==1);armed[1]=true;turn_based=true;activations++;}
static void mes_get_msg(int file,MesFileEntry* entry){(void)file;assert(entry->num==24);entry->str="under attack";}
static void intgame_message_window_display_msg(UiMessage* message){assert(message->type==UI_MSG_TYPE_FEEDBACK);feedback++;}
static void sub_5517F0(void){}
static int64_t object_hover_obj_get(void){return 0;}
static void object_hover_obj_set(int64_t obj){(void)obj;}
static void sub_57CCF0(int64_t pc,int64_t obj){(void)pc;(void)obj;}
static int obj_field_int32_get(int64_t obj,int field){(void)obj;return field==OBJ_F_SPELL_FLAGS?spell_flags:critter_flags;}
static bool anim_is_current_goal_type(int64_t obj,int goal,void* id){(void)obj;(void)goal;(void)id;return fidget;}
static bool sub_423300(int64_t obj,void* id){(void)obj;(void)id;return busy;}
static bool critter_is_concealed(int64_t obj){(void)obj;return concealed;}
static void critter_set_concealed(int64_t obj,bool value){(void)obj;concealed=value;}
'''+toggle+'\nstatic void right_click(int64_t pc_obj){\n'+right_click+'}\n'+r'''
int main(void){
 for(int actor=1;actor<=3;actor++)for(int drawn=0;drawn<=1;drawn++){
  armed[actor]=drawn;armed[1]=true;turn_based=true;
  right_click(actor);assert(!turn_based&&!armed[1]&&!activations);
 }
 armed[1]=true;armed[2]=false;turn_based=true;under_attack=true;
 right_click(2);assert(turn_based&&armed[1]&&feedback==1);
 under_attack=false;busy=true;
 right_click(2);assert(turn_based);
 fidget=true;right_click(2);assert(!turn_based);busy=fidget=false;
 armed[1]=true;turn_based=true;
 active=false;right_click(2);assert(turn_based);active=true;
 prone=true;right_click(2);assert(turn_based);prone=false;
 spell_flags=OSF_STONED;right_click(2);assert(turn_based);spell_flags=0;
 critter_flags=OCF_PARALYZED;right_click(2);assert(turn_based);
 critter_flags=OCF_STUNNED;right_click(2);assert(turn_based);critter_flags=0;
 right_click(2);assert(!turn_based);
 concealed=true;right_click(1);assert(!concealed&&!turn_based&&!activations);
 puts("PASS: exit from idle/drawn companions and hero; hostile, animation and incapacitation guards; ordinary right-click does not start combat");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-combat-exit-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
