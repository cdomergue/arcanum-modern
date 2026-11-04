#!/usr/bin/env python3
"""Check combat chords, end-turn separation and physical key ownership."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
ui = (root / 'source/src/ui/modern_ui.c').read_text()
keys = (root / 'source/src/ui/modern_keys.inc').read_text()

def function(source, signature):
    start = source.index(signature + '\n{')
    return source[start:source.index('\n}', start) + 2]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
typedef int SDL_Scancode;
enum {SDL_BUTTON_MIDDLE=2};
enum {SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_SPACE=44, SDL_SCANCODE_COMMA=54, SDL_SCANCODE_PERIOD=55, SDL_SCANCODE_SLASH=56, SDL_SCANCODE_COUNT=512};
enum {SDL_KMOD_SHIFT=3, SDL_KMOD_CTRL=192, SDL_KMOD_ALT=768, SDL_KMOD_GUI=3072};
enum {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, INTGAME_MODE_DIALOG=10};
typedef struct {int type; struct {int scancode,mod; bool repeat;} key;} SDL_Event;
static bool active,panel,typing;
static int mode,pending_action=-1,tactical_pressed_actor;
static void* window;
static bool SDL_TextInputActive(void* w){(void)w;return typing;}
static bool modern_camera_enabled(void){return !panel;}
static bool follow_camera;
static unsigned follow_walk_taps;
static bool modern_follow_camera_active(void){return follow_camera && !active && !panel;}
static bool native_panel(void){return panel;}
static int intgame_mode_get(void){return mode;}
static bool tactical_active(void){return active;}
static int combat_input_actor(void){return 42;}
static bool tig_movie_is_playing(void){return false;}
static bool tactical_camera_event(SDL_Event* e){(void)e;return false;}
static void hotbar_event(SDL_Event* e){(void)e;}
'''
code += 'enum {' + ','.join(f'SDL_SCANCODE_{chr(65+i)}={i+4}' for i in range(26)) + '};\n'
code += 'enum {' + ','.join(f'SDL_SCANCODE_{(i+1)%10}={i+30}' for i in range(10)) + '};\n'
code += "static int SDL_GetScancodeFromKey(int key,void* mods){(void)mods;return key-'a'+SDL_SCANCODE_A;}\n"
code += keys[keys.index('enum {KB_'):keys.index('static const char* tr(')]
code += keys[keys.index('static int key_chord'):keys.index('static const char* key_name')]
code += function(keys, 'static int key_default(int i)')
code += function(ui, 'static bool handle_binding_event(SDL_Event* event)')
code += '\nstatic bool handle_ui_event(SDL_Event* e){return handle_binding_event(e);}\n'
code += function(ui, 'static bool handle_event(SDL_Event* event)')
code += r'''
static bool press(int key,int mods){
    SDL_Event e={.type=SDL_EVENT_KEY_DOWN,.key={key,mods,false}};
    pending_action=-1;
    bool consumed=handle_event(&e);
    e.type=SDL_EVENT_KEY_UP;handle_event(&e);
    return consumed;
}
int main(void){
    memcpy(key_binding,key_defaults,sizeof(key_binding));
    assert(press(SDL_SCANCODE_W,0) && follow_walk_taps==0);
    follow_camera=true;
    assert(press(SDL_SCANCODE_W,0) && follow_walk_taps==1);
    follow_walk_taps=0;
    assert(press(SDL_SCANCODE_R,0) && pending_action==7);
    assert(press(SDL_SCANCODE_L,0) && pending_action==0);
    assert(press(SDL_SCANCODE_G,0) && pending_action==3);
    assert(press(SDL_SCANCODE_M,0) && pending_action==6);
    assert(press(SDL_SCANCODE_H,0) && pending_action==9);
    assert(press(SDL_SCANCODE_H,1) && pending_action==951);
    assert(press(SDL_SCANCODE_A,1) && pending_action==952);
    assert(press(SDL_SCANCODE_1,0) && pending_action==300);
    assert(press(SDL_SCANCODE_1,SDL_KMOD_SHIFT) && pending_action==310);
    assert(press(SDL_SCANCODE_0,SDL_KMOD_SHIFT) && pending_action==319);
    assert(!press(SDL_SCANCODE_COMMA,0));
    assert(!press(SDL_SCANCODE_PERIOD,0));
    assert(!press(SDL_SCANCODE_SLASH,0));
    active=true;
    assert(press(SDL_SCANCODE_W,0) && follow_walk_taps==0);
    assert(press(SDL_SCANCODE_R,0) && pending_action==7);
    assert(!press(SDL_SCANCODE_SPACE,0) && pending_action==-1);
    assert(press(SDL_SCANCODE_E,0) && pending_action==1100);
    assert(tactical_pressed_actor==42);
    panel=true;assert(press(SDL_SCANCODE_R,0) && pending_action==-1);panel=false;
    mode=INTGAME_MODE_DIALOG;assert(press(SDL_SCANCODE_R,0) && pending_action==-1);mode=0;
    typing=true;assert(!press(SDL_SCANCODE_R,0) && pending_action==-1);typing=false;
    assert(!press(SDL_SCANCODE_R,SDL_KMOD_CTRL) && pending_action==-1);
    key_binding[KB_COMBAT]=SDL_SCANCODE_E|KEY_SHIFT;
    assert(press(SDL_SCANCODE_E,1) && pending_action==7);
    assert(press(SDL_SCANCODE_E,0) && pending_action==1100);
    assert(press(SDL_SCANCODE_R,0) && pending_action==-1);
    key_binding[KB_COMBAT]=0;
    assert(press(SDL_SCANCODE_R,0) && pending_action==-1);
    memcpy(key_binding,key_defaults,sizeof(key_binding));
    SDL_Event e={.type=SDL_EVENT_KEY_DOWN,.key={SDL_SCANCODE_R,0,false}};
    assert(handle_event(&e) && pending_action==7);pending_action=-1;
    e.key.repeat=true;e.key.mod=SDL_KMOD_SHIFT;
    assert(handle_event(&e) && pending_action==-1);
    e.type=SDL_EVENT_KEY_UP;assert(handle_event(&e) && pending_action==-1);
    assert(!press(SDL_SCANCODE_SPACE,0) && pending_action==-1);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-combat-binding-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-Wno-unused-variable', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: original actions, movement exceptions, rebinding, native Space, modifiers, panels and held-key release')
