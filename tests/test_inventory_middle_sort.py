#!/usr/bin/env python3
"""Inventory sorting bindings respect input context and reuse arrange dispatch."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_panels.inc').read_text()
function=re.search(r'^static bool inventory_sort_event\(SDL_Event\* event\)\n\{.*?^\}',s,re.M|re.S).group()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP,SDL_EVENT_MOUSE_WHEEL,SDL_EVENT_KEY_DOWN,SDL_EVENT_KEY_UP};
enum {KB_SORT,KEY_MOUSE=1<<17,KEY_SHIFT=1<<16,SDL_KMOD_SHIFT=3,SDL_KMOD_CTRL=192,SDL_KMOD_ALT=768,SDL_KMOD_GUI=3072};
enum {SDL_BUTTON_LEFT=1,SDL_BUTTON_MIDDLE=2,SDL_BUTTON_RIGHT=3,SDL_BUTTON_X1=4,SDL_BUTTON_X2=5,INTGAME_MODE_QUANTITY=99};
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {int type;struct {int button;float x,y;} button;struct {int scancode,mod;bool repeat;} key;} SDL_Event;
typedef struct {int mode;int64_t pc,target;} ModernInventory;
static int window,panel=2,mode,pending_action=-1,width=800,height=450;
static float canvas_w=1600,canvas_h=900;
static SDL_FRect inventory_rect={100,100,1400,700};
static bool inventory_sort_pressed,typing,binding_pressed,inventory_quick_release,valid=true;
static int64_t inventory_sort_pc,inventory_sort_target,drag;
static int inventory_sort_mode,inventory_sort_button,mods;
static int key_binding[1]={KEY_MOUSE|SDL_BUTTON_MIDDLE};
static int SDL_GetModState(void){return mods;}
static bool SDL_TextInputActive(int w){(void)w;return typing;}
static bool key_is(int i,int key,int mod){return key_binding[i] && key_binding[i]==(key|((mod&SDL_KMOD_SHIFT)?KEY_SHIFT:0));}
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
static void blocked(void){pending_action=-1;assert(inventory_sort_event(&down));assert(inventory_sort_event(&up));assert(pending_action==-1);}
int main(void){
 assert(inventory_sort_event(&down));assert(inventory_sort_pressed&&pending_action==-1);
 SDL_Event motion={SDL_EVENT_MOUSE_WHEEL,{0}};assert(!inventory_sort_event(&motion));assert(inventory_sort_pressed);
 assert(inventory_sort_event(&up));assert(pending_action==731&&!inventory_sort_pressed);
 pending_action=-1;assert(inventory_sort_event(&up));assert(pending_action==-1);
 key_binding[KB_SORT]=0;blocked();key_binding[KB_SORT]=KEY_MOUSE|SDL_BUTTON_MIDDLE;
 drag=20;blocked();drag=0;binding_pressed=true;blocked();binding_pressed=false;
 inventory_quick_release=true;blocked();inventory_quick_release=false;
 mode=INTGAME_MODE_QUANTITY;blocked();mode=0;valid=false;blocked();valid=true;
 width=0;blocked();width=800;
 down.button.x=0;blocked();down.button.x=200;
 inventory_sort_event(&down);up.button.x=0;assert(inventory_sort_event(&up)&&pending_action==-1);up.button.x=200;
 inventory_sort_event(&down);current.pc=3;assert(inventory_sort_event(&up)&&pending_action==-1);current.pc=1;
 inventory_sort_event(&down);current.target=4;assert(inventory_sort_event(&up)&&pending_action==-1);current.target=2;
 inventory_sort_event(&down);current.mode=5;assert(inventory_sort_event(&up)&&pending_action==-1);current.mode=0;
 inventory_sort_event(&down);SDL_Event lost={.type=SDL_EVENT_WINDOW_FOCUS_LOST};inventory_sort_event(&lost);assert(inventory_sort_event(&up)&&pending_action==-1);
 inventory_sort_event(&down);menu=true;assert(inventory_sort_event(&up)&&pending_action==-1);assert(!inventory_sort_event(&down));menu=false;
 inventory_sort_event(&down);slide=true;assert(inventory_sort_event(&up)&&pending_action==-1);slide=false;
 inventory_sort_event(&down);panel=0;assert(inventory_sort_event(&up)&&pending_action==-1);assert(!inventory_sort_event(&down));panel=2;
 down.button.button=SDL_BUTTON_LEFT;assert(!inventory_sort_event(&down));down.button.button=SDL_BUTTON_RIGHT;assert(!inventory_sort_event(&down));
 key_binding[KB_SORT]=KEY_MOUSE|SDL_BUTTON_X1;
 down.button.button=SDL_BUTTON_MIDDLE;blocked();
 down.button.button=up.button.button=SDL_BUTTON_X1;
 assert(inventory_sort_event(&down));assert(inventory_sort_event(&up)&&pending_action==731);pending_action=-1;
 key_binding[KB_SORT]|=KEY_SHIFT;blocked();mods=SDL_KMOD_SHIFT;
 assert(inventory_sort_event(&down));assert(inventory_sort_event(&up)&&pending_action==731);pending_action=-1;
 mods|=SDL_KMOD_CTRL;blocked();mods=0;
 key_binding[KB_SORT]=5;
 SDL_Event key={.type=SDL_EVENT_KEY_DOWN,.key={5,0,false}};
 assert(inventory_sort_event(&key)&&pending_action==731);pending_action=-1;
 key.key.repeat=true;assert(inventory_sort_event(&key)&&pending_action==-1);key.key.repeat=false;
 key.type=SDL_EVENT_KEY_UP;assert(inventory_sort_event(&key)&&pending_action==-1);key.type=SDL_EVENT_KEY_DOWN;
 typing=true;assert(!inventory_sort_event(&key)&&pending_action==-1);typing=false;
 mode=INTGAME_MODE_QUANTITY;assert(inventory_sort_event(&key)&&pending_action==-1);mode=0;
 drag=20;assert(inventory_sort_event(&key)&&pending_action==-1);drag=0;
 menu=true;assert(!inventory_sort_event(&key)&&pending_action==-1);menu=false;
 panel=0;assert(!inventory_sort_event(&key)&&pending_action==-1);panel=2;
 key.key.mod=SDL_KMOD_CTRL;assert(!inventory_sort_event(&key)&&pending_action==-1);
 puts("PASS: keyboard/mouse bindings, modifiers, cleared binding, drags, quantity, focus, owner/panel changes and typing");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-middle-sort-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-missing-field-initializers','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
