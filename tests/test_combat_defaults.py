#!/usr/bin/env python3
"""Verify fresh installs and imported real-time settings before PC creation."""
from pathlib import Path
import os, re, subprocess, tempfile

source = (Path(__file__).resolve().parents[1]/'source/src/game/combat.c').read_text()
def function(name):
    m = re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{', source, re.M)
    assert m, name
    return source[m.start():source.index('\n}', m.end())+2]+'\n'
assert 'settings_register(&settings, TURN_BASED_KEY, "1", turn_based_changed)' in source
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define TURN_BASED_KEY 0
#define FAST_TURN_BASED_KEY 1
static bool tactical_enabled,combat_turn_based,combat_fast_turn_based=true;
static int settings,values[2]={0,1},interrupts;
static int64_t pc;
static struct {void(*field_4)(void);void(*field_8)(void);} combat_callbacks;
static int64_t player_get_local_pc_obj(void){return pc;}
static bool sub_424070(int64_t obj,int a,int b,int c){assert(obj);(void)a;(void)b;(void)c;interrupts++;return true;}
static bool combat_critter_is_combat_mode_active(int64_t obj){(void)obj;return false;}
static void combat_turn_based_end(void){}
static bool combat_turn_based_start(void){return true;}
static int settings_get_value(int* s,int key){(void)s;return values[key];}
void turn_based_changed(void);void fast_turn_based_changed(void);
static void settings_set_value(int* s,int key,int value){
 (void)s;values[key]=value;
 if(key==TURN_BASED_KEY)turn_based_changed();else fast_turn_based_changed();
}
''' + ''.join(function(n) for n in ['sub_4B6C90','turn_based_changed','fast_turn_based_changed','combat_tactical_enable']) + r'''
int main(void){
 /* Imported real-time + fast-turn preferences, while still on the main menu. */
 combat_tactical_enable();
 assert(tactical_enabled&&combat_turn_based&&!combat_fast_turn_based);
 assert(values[0]==1&&values[1]==0&&interrupts==0);
 /* A stale option cannot restore real time, before or after creating a PC. */
 settings_set_value(&settings,0,0);assert(combat_turn_based);
 pc=1;settings_set_value(&settings,0,0);assert(combat_turn_based);
 settings_set_value(&settings,1,1);assert(!combat_fast_turn_based);
 puts("PASS: fresh-install default, pre-PC initialization and imported real-time/fast settings");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-defaults-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
