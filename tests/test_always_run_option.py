#!/usr/bin/env python3
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / 'source/src'

def function(path, name):
    source = (root / path).read_text()
    match = re.search(r'^(?:bool|void) ' + name + r'\([^\n]*\)\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index('\n}', match.end()) + 2]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {ALWAYS_RUN_KEY,SDL_KMOD_CTRL=1,SDL_KMOD_NUM=2,MULTIPLAYER_ALWAYS_RUN=4};
static int settings,stored,modifiers,network_flags;
static bool network;
static int64_t player_get_local_pc_obj(void){return 1;}
static bool combat_player_controls(int64_t obj){return obj==1;}
static bool tig_net_is_active(void){return network;}
static int multiplayer_find_slot_from_obj(int64_t obj){return obj==1?0:-1;}
static int multiplayer_flags_get(int slot){assert(slot==0);return network_flags;}
static void multiplayer_flags_set(int slot,int value){assert(slot==0);network_flags|=value;}
static void multiplayer_flags_unset(int slot,int value){assert(slot==0);network_flags&=~value;}
static int settings_get_value(int* s,int key){assert(s==&settings&&key==ALWAYS_RUN_KEY);return stored;}
static void settings_set_value(int* s,int key,int value){assert(s==&settings&&key==ALWAYS_RUN_KEY);stored=value;}
static bool tig_kb_get_modifier(int flag){return (modifiers&flag)!=0;}
''' + '\n'.join([
    function('game/anim.c', 'get_always_run'),
    function('game/anim.c', 'set_always_run'),
    function('ui/options_ui.c', 'options_ui_awlays_run_get'),
    function('ui/options_ui.c', 'options_ui_always_run_set'),
]) + r'''
int main(void){
    int value;bool enabled;
    for(int mode=0;mode<2;mode++){
        network=mode;
        for(modifiers=0;modifiers<4;modifiers++)for(int choice=0;choice<2;choice++){
            options_ui_always_run_set(choice);
            for(int reopen=0;reopen<3;reopen++){
                options_ui_awlays_run_get(&value,&enabled);
                assert(enabled&&value==choice&&stored==choice);
            }
        }
    }
    network=false;modifiers=0;options_ui_always_run_set(false);assert(!get_always_run(1));
    modifiers=SDL_KMOD_CTRL;assert(get_always_run(1));
    options_ui_awlays_run_get(&value,&enabled);assert(value==0);
    options_ui_always_run_set(true);assert(!get_always_run(1));
    options_ui_awlays_run_get(&value,&enabled);assert(value==1);
    puts("PASS: persistent run preference, Ctrl/NumLock-independent option display, reopen and multiplayer flags");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-always-run-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
