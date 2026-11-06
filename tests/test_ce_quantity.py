"""Exercise native inventory dispatch and the production portrait mouse branch."""
from pathlib import Path
import os,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]/'source/src/ui'
s=(r/'inven_ui.c').read_text();start=s.index('bool inven_ui_message_filter(TigMessage* msg)\n{');inventory=s[start:s.index('\n}',start)+2]
s=(r/'follower_ui.c').read_text();start=s.index('bool follower_ui_message_filter(TigMessage* msg)\n{');body=s[start:s.index('\n}',start)+2]
start=body.rindex('    case TIG_MESSAGE_MOUSE:');portrait=body[start:body.index('    default:',start)]
constants=sorted(set(re.findall(r'\b(?:TIG_MESSAGE_|TIG_BUTTON_STATE_|INVEN_UI_MODE_|INTGAME_MODE_|GRAVITY_)[A-Z_]+',inventory+portrait)))
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define TIG_OK 0
#define FOLLOWER_UI_SLOTS 2
typedef int tig_window_handle_t;
typedef struct {int x,y,width,height;} TigRect;
typedef struct {int type;struct {struct {int x,y,event;} mouse;struct {int state,button_handle;} button;} data;} TigMessage;
'''+ 'enum {'+','.join(constants)+'};\n'+r'''
static int mode,calls,closed,menu_open,top_window=10;
static int inven_ui_drag_item_obj,inven_ui_mode,dword_68346C,dword_739F58,qword_682C78;
static int follower_ui_top_index,follower_ui_followers_count=1;
static bool follower_ui_visible=true;
static int follower_ui_windows[]={10,11};
static TigRect follower_ui_button_rects[]={{0,0,30,30},{0,30,30,30}};
static int intgame_mode_get(void){return mode;}
static void hrp_apply(TigRect* r,int flags){(void)r;(void)flags;}
static void scrollbar_ui_begin_ignore_events(void){}
static void scrollbar_ui_end_ignore_events(void){}
static bool intgame_hotkey_is_dragging(void){return false;}
static bool critter_is_sleeping(int obj){(void)obj;return false;}
static bool critter_is_active(int obj){(void)obj;return true;}
static void inven_ui_destroy(void){closed++;}
static int tig_window_get_at_position(int x,int y,int* out){(void)x;(void)y;*out=top_window;return TIG_OK;}
static void follower_ui_drop_down_menu_create(int index){assert(index==0);menu_open++;}
'''
for name in sorted(set(re.findall(r'\b(inven_ui_\w+handle_\w+)\(',inventory))):
 args='int button' if 'button_' in name and 'mouse_' not in name else 'int x,int y'
 if name.endswith(('lbutton_up','rbutton_up')):args+=',bool* out'
 unused=''.join('(void)'+arg.split()[-1].lstrip('*')+';' for arg in args.split(','))
 prefix+=f'static bool {name}({args}){{{unused}calls++;return true;}}\n'
code=prefix+inventory+'\nstatic bool portrait_mouse(TigMessage* msg){int index;switch(msg->type){\n'+portrait+'default:return false;}}\n'+r'''
int main(void){
 TigMessage msg={0};inven_ui_mode=INVEN_UI_MODE_LOOT;
 int events[]={TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP,TIG_MESSAGE_MOUSE_MOVE};
 mode=INTGAME_MODE_QUANTITY;
 for(unsigned i=0;i<sizeof(events)/sizeof(events[0]);i++){
  msg.type=TIG_MESSAGE_MOUSE;msg.data.mouse.event=events[i];assert(!inven_ui_message_filter(&msg));assert(calls==0&&closed==0);
 }
 msg.type=TIG_MESSAGE_BUTTON;msg.data.button.state=TIG_BUTTON_STATE_RELEASED;
 assert(!inven_ui_message_filter(&msg)&&calls==0);
 mode=-1;assert(inven_ui_message_filter(&msg)&&calls==1);
 puts("PASS: native quantity routing");
}
'''
with tempfile.TemporaryDirectory(prefix='ce-input-') as folder:
 p=Path(folder)/'test';p.with_suffix('.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable',str(p.with_suffix('.c')),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
