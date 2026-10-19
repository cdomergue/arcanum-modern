#!/usr/bin/env python3
"""Exercise physical camera keys and modal/focus/text-input exclusions."""
from pathlib import Path
import os
import subprocess
import tempfile

source = (Path(__file__).resolve().parents[1] / 'source/src/ui/modern_ui.c').read_text()
start = source.index('static bool modern_camera_enabled(void)')
end = source.index('static bool handle_ui_event(SDL_Event* event)\n{', start)
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
enum { INTGAME_MODE_MAIN, INTGAME_MODE_SPELL, INTGAME_MODE_SKILL, INTGAME_MODE_ITEM,
       INTGAME_MODE_DIALOG, SDL_SCANCODE_COUNT=512 };
enum {SDL_SCANCODE_W=26,SDL_SCANCODE_A=4,SDL_SCANCODE_S=22,SDL_SCANCODE_D=7,KB_UP=0};
static int key_binding[]={26,4,22,7};
enum { SDL_KMOD_CTRL=1, SDL_KMOD_ALT=2, SDL_KMOD_GUI=4 };
static bool enabled=true, in_game=true, journal, searching, menu_suspended;
static bool menu, panel, slide, typing, keys[512];
static int mode, mods, dummy;
static void *window=&dummy, *focus=&dummy;
static int intgame_mode_get(void){return mode;}
static bool mainmenu_ui_is_active(void){return menu;}
static bool native_panel(void){return panel;}
static bool slide_ui_is_active(void){return slide;}
static void *SDL_GetKeyboardFocus(void){return focus;}
static bool SDL_TextInputActive(void *w){(void)w;return typing;}
static int SDL_GetModState(void){return mods;}
static bool *SDL_GetKeyboardState(void *n){(void)n;return keys;}
''' + source[start:end] + r'''
int main(void){
    keys[26]=true; /* SDL physical W: Z on an AZERTY layout. */
    assert(modern_camera_key(26));assert(!modern_camera_key(4));
    assert(!modern_camera_key(-1));assert(!modern_camera_key(512));
    bool *blocked[]={&journal,&searching,&menu_suspended,&menu,&panel,&slide,&typing};
    for(unsigned i=0;i<sizeof(blocked)/sizeof(*blocked);i++){
        *blocked[i]=true;assert(!modern_camera_key(26));*blocked[i]=false;
    }
    enabled=false;assert(!modern_camera_key(26));enabled=true;
    in_game=false;assert(!modern_camera_key(26));in_game=true;
    focus=NULL;assert(!modern_camera_key(26));focus=window;
    for(mods=1;mods<=4;mods*=2){assert(!modern_camera_key(26));}
    mods=0;
    for(mode=0;mode<=INTGAME_MODE_ITEM;mode++)assert(modern_camera_key(26));
    assert(!modern_camera_key(26));
    keys[26]=false;mode=0;assert(!modern_camera_key(26));
    key_binding[0]=29;keys[29]=true;assert(modern_camera_key(26));
    key_binding[0]=0;assert(!modern_camera_key(26));
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-camera-') as folder:
    path=Path(folder)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path/'test.c'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
print('PASS: physical key state, release, modal panels, text input, focus and modifiers')
