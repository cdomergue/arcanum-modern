#!/usr/bin/env python3
"""Middle clicks reuse arrange dispatch without falling through to item clicks."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_panels.inc').read_text()
function=re.search(r'^static bool inventory_middle_event\(SDL_Event\* event\)\n\{.*?^\}',s,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP,SDL_EVENT_MOUSE_WHEEL};
enum {SDL_BUTTON_LEFT=1,SDL_BUTTON_MIDDLE=2,SDL_BUTTON_RIGHT=3,INTGAME_MODE_QUANTITY=99};
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {int type;struct {int button;float x,y;} button;} SDL_Event;
typedef struct {int mode;int64_t pc,target;} ModernInventory;
static int window,panel=2,mode,pending_action=-1,width=800,height=450;
static float canvas_w=1600,canvas_h=900;
static SDL_FRect inventory_rect={100,100,1400,700};
static bool inventory_middle_pressed,inventory_middle_sort=true,binding_pressed,inventory_quick_release,valid=true;
static int64_t inventory_middle_pc,inventory_middle_target,drag;
static int inventory_middle_mode;
static ModernInventory current={0,1,2};
static bool menu,slide;
static bool native_menu(void){return menu;}
static bool slide_ui_is_active(void){return slide;}
static int native_panel(void){return panel;}
static void SDL_GetWindowSize(int w,int* x,int* y){(void)w;*x=width;*y=height;}
static bool inside(SDL_FRect r,float x,float y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
static int intgame_mode_get(void){return mode;}
static int64_t inven_ui_drag_item_obj_get(void){return drag;}
static bool modern_inventory_state(ModernInventory* out){*out=current;return valid;}
'''+function+r'''
static SDL_Event down={SDL_EVENT_MOUSE_BUTTON_DOWN,{SDL_BUTTON_MIDDLE,200,150}};
static SDL_Event up={SDL_EVENT_MOUSE_BUTTON_UP,{SDL_BUTTON_MIDDLE,200,150}};
static void blocked(void){pending_action=-1;assert(inventory_middle_event(&down));assert(inventory_middle_event(&up));assert(pending_action==-1);}
int main(void){
 assert(inventory_middle_event(&down));assert(inventory_middle_pressed&&pending_action==-1);
 SDL_Event motion={SDL_EVENT_MOUSE_WHEEL,{0}};assert(!inventory_middle_event(&motion));assert(inventory_middle_pressed);
 assert(inventory_middle_event(&up));assert(pending_action==731&&!inventory_middle_pressed);
 pending_action=-1;assert(inventory_middle_event(&up));assert(pending_action==-1);
 inventory_middle_sort=false;blocked();inventory_middle_sort=true;
 drag=20;blocked();drag=0;binding_pressed=true;blocked();binding_pressed=false;
 inventory_quick_release=true;blocked();inventory_quick_release=false;
 mode=INTGAME_MODE_QUANTITY;blocked();mode=0;valid=false;blocked();valid=true;
 width=0;blocked();width=800;
 down.button.x=0;blocked();down.button.x=200;
 inventory_middle_event(&down);up.button.x=0;assert(inventory_middle_event(&up)&&pending_action==-1);up.button.x=200;
 inventory_middle_event(&down);current.pc=3;assert(inventory_middle_event(&up)&&pending_action==-1);current.pc=1;
 inventory_middle_event(&down);current.target=4;assert(inventory_middle_event(&up)&&pending_action==-1);current.target=2;
 inventory_middle_event(&down);current.mode=5;assert(inventory_middle_event(&up)&&pending_action==-1);current.mode=0;
 inventory_middle_event(&down);SDL_Event lost={.type=SDL_EVENT_WINDOW_FOCUS_LOST};inventory_middle_event(&lost);assert(inventory_middle_event(&up)&&pending_action==-1);
 inventory_middle_event(&down);menu=true;assert(inventory_middle_event(&up)&&pending_action==-1);assert(!inventory_middle_event(&down));menu=false;
 inventory_middle_event(&down);slide=true;assert(inventory_middle_event(&up)&&pending_action==-1);slide=false;
 inventory_middle_event(&down);panel=0;assert(inventory_middle_event(&up)&&pending_action==-1);assert(!inventory_middle_event(&down));panel=2;
 down.button.button=SDL_BUTTON_LEFT;assert(!inventory_middle_event(&down));down.button.button=SDL_BUTTON_RIGHT;assert(!inventory_middle_event(&down));
 puts("PASS: middle press/release, disabled shortcut, drags, quantity, focus, owner/panel changes and no legacy left-click fallback");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-middle-sort-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
