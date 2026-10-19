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
enum {SDL_SCANCODE_UNKNOWN, SDL_SCANCODE_SPACE=44, SDL_SCANCODE_COUNT=512};
enum {SDL_KMOD_SHIFT=3, SDL_KMOD_CTRL=192, SDL_KMOD_ALT=768, SDL_KMOD_GUI=3072};
enum {SDL_EVENT_KEY_DOWN, SDL_EVENT_KEY_UP, INTGAME_MODE_DIALOG=10};
typedef struct {int type; struct {int scancode,mod; bool repeat;} key;} SDL_Event;
static bool active,panel,typing;
static int mode,pending_action=-1,tactical_pressed_actor;
static void* window;
static bool SDL_TextInputActive(void* w){(void)w;return typing;}
static bool modern_camera_enabled(void){return !panel;}
static bool native_panel(void){return panel;}
static int intgame_mode_get(void){return mode;}
static bool tactical_active(void){return active;}
static int combat_input_actor(void){return 42;}
static bool tig_movie_is_playing(void){return false;}
static bool tactical_camera_event(SDL_Event* e){(void)e;return false;}
'''
code += 'enum {' + ','.join(f'SDL_SCANCODE_{chr(65+i)}={i+4}' for i in range(26)) + '};\n'
code += keys[keys.index('enum {KB_'):keys.index('static const char* tr(')]
code += keys[keys.index('static int key_chord'):keys.index('static const char* key_name')]
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
    assert(press(SDL_SCANCODE_SPACE,1) && pending_action==7);
    assert(press(SDL_SCANCODE_SPACE,2) && pending_action==7);
    active=true;
    assert(press(SDL_SCANCODE_SPACE,SDL_KMOD_SHIFT) && pending_action==7);
    assert(press(SDL_SCANCODE_SPACE,0) && pending_action==1100);
    assert(press(SDL_SCANCODE_E,0) && pending_action==1100);
    assert(tactical_pressed_actor==42);
    panel=true;assert(press(SDL_SCANCODE_SPACE,1) && pending_action==-1);panel=false;
    mode=INTGAME_MODE_DIALOG;assert(press(SDL_SCANCODE_SPACE,1) && pending_action==-1);mode=0;
    typing=true;assert(!press(SDL_SCANCODE_SPACE,1) && pending_action==-1);typing=false;
    assert(!press(SDL_SCANCODE_SPACE,SDL_KMOD_CTRL|1) && pending_action==-1);
    key_binding[KB_COMBAT]=SDL_SCANCODE_E|KEY_SHIFT;
    assert(press(SDL_SCANCODE_E,1) && pending_action==7);
    assert(press(SDL_SCANCODE_E,0) && pending_action==1100);
    assert(press(SDL_SCANCODE_SPACE,1) && pending_action==-1);
    key_binding[KB_COMBAT]=0;
    assert(press(SDL_SCANCODE_SPACE,1) && pending_action==-1);
    memcpy(key_binding,key_defaults,sizeof(key_binding));
    SDL_Event e={.type=SDL_EVENT_KEY_DOWN,.key={SDL_SCANCODE_SPACE,1,false}};
    assert(handle_event(&e) && pending_action==7);pending_action=-1;
    e.key.repeat=true;e.key.mod=0;
    assert(handle_event(&e) && pending_action==-1);
    e.type=SDL_EVENT_KEY_UP;assert(handle_event(&e) && pending_action==-1);
    assert(press(SDL_SCANCODE_SPACE,0) && pending_action==1100);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-combat-binding-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-Wno-unused-variable', str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('PASS: Shift+Space, rebinding, end-turn separation, modifiers, panels and held-key release')
