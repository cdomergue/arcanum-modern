#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_panels.inc').read_text()
start = source.index('static bool binding_event(SDL_Event* event)')
function = source[start:source.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {SDL_FRect rect;int action;bool enabled;} Button;
typedef struct {int type,data,slot;int64_t item,actor;} QuickBinding;
typedef struct {int type;struct {int scancode;bool repeat;} key;struct {int button;} button;} SDL_Event;
enum {SDL_EVENT_WINDOW_FOCUS_LOST,SDL_EVENT_KEY_DOWN,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_UP};
enum {SDL_SCANCODE_1=30,SDL_SCANCODE_0=39,SDL_SCANCODE_ESCAPE=41};
enum {HOTKEY_NONE,HOTKEY_ITEM,HOTKEY_SKILL,HOTKEY_SPELL,INTGAME_MODE_QUANTITY=8,SDL_BUTTON_LEFT=1};
static QuickBinding binding_drag,binding_pending;
static bool binding_pressed,binding_moved,binding_cancel,click_captured,captured;
static float mouse_x,mouse_y,binding_x,binding_y;
static int binding_action=-1,pressed_action=-1,pending_action=-1,source_type=HOTKEY_SKILL;
static Button buttons[]={{{10,10,180,50},880,true}};
static int button_count=1;
static bool inside(SDL_FRect r,float x,float y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static int intgame_mode_get(void){return 0;}
static void SDL_CaptureMouse(bool value){captured=value;}
static QuickBinding binding_hover(void){return (QuickBinding){inside(buttons[0].rect,mouse_x,mouse_y)?source_type:HOTKEY_NONE,2,-1,0,7};}
static int binding_slot_at(float x,float y){return inside((SDL_FRect){300,100,50,45},x,y)?9:-1;}
''' + function + r'''
static bool event(int type,float x,float y){mouse_x=x;mouse_y=y;SDL_Event e={.type=type};e.button.button=SDL_BUTTON_LEFT;return binding_event(&e);}
static void reset(void){binding_drag=(QuickBinding){0};binding_pending=(QuickBinding){0};binding_pressed=binding_moved=binding_cancel=click_captured=captured=false;pending_action=pressed_action=binding_action=-1;}
int main(void){
    for(int type=HOTKEY_SKILL;type<=HOTKEY_SPELL;type++) {
        reset();source_type=type;
        assert(event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30));assert(captured&&binding_pressed&&pending_action==-1);
        for(int frame=0;frame<30;frame++){event(SDL_EVENT_MOUSE_MOTION,31,30);assert(!binding_moved&&pending_action==-1&&!binding_pending.type);}
        event(SDL_EVENT_MOUSE_MOTION,325,120);assert(binding_moved&&!binding_pending.type);
        for(int frame=0;frame<30;frame++)assert(pending_action==-1);
        assert(event(SDL_EVENT_MOUSE_BUTTON_UP,325,120));
        assert(!captured&&!binding_pressed&&pending_action==-1&&binding_pending.slot==9&&binding_pending.type==type&&binding_pending.actor==7);
        reset();assert(event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30));
        assert(event(SDL_EVENT_MOUSE_BUTTON_UP,325,120));assert(binding_pending.type==type&&pending_action==-1);
    }
    reset();assert(event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30));assert(event(SDL_EVENT_MOUSE_BUTTON_UP,31,30));assert(pending_action==880&&!binding_pending.type&&!captured);
    reset();assert(event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30));event(SDL_EVENT_MOUSE_MOTION,230,70);assert(event(SDL_EVENT_MOUSE_BUTTON_UP,230,70));assert(pending_action==-1&&!binding_pending.type&&!captured);
    reset();event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30);SDL_Event escape={.type=SDL_EVENT_KEY_DOWN,.key={SDL_SCANCODE_ESCAPE,false}};assert(binding_event(&escape));assert(binding_cancel&&!binding_pressed&&!captured&&pending_action==-1);
    reset();event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30);event(SDL_EVENT_WINDOW_FOCUS_LOST,30,30);assert(binding_cancel&&!binding_pressed&&!captured);
    reset();source_type=HOTKEY_NONE;assert(!event(SDL_EVENT_MOUSE_BUTTON_DOWN,30,30));assert(!captured&&!binding_pressed);
    puts("PASS: skill/spell drag, held press, delayed release, coalesced motion, click, outside drop, Escape, focus loss and unavailable source");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-binding-drag-') as folder:
    path=Path(folder)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
