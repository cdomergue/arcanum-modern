#!/usr/bin/env python3
"""Exercise passive hover capture without repeating examination side effects."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/intgame.c').read_text()
header = (root / 'source/src/ui/modern_ui.h').read_text()

def function(name):
    match = re.search(r'^(?:void|const ModernDetails\*) ' + name + r'\([^\n]*\)\n\{', source, re.M)
    end = match.end()
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]

types = ['WALL', 'PORTAL', 'CONTAINER', 'SCENERY', 'PROJECTILE', 'WEAPON', 'AMMO',
         'ARMOR', 'GOLD', 'FOOD', 'SCROLL', 'KEY', 'KEY_RING', 'WRITTEN', 'GENERIC', 'PC', 'NPC']
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_TYPE 0
enum {ROTWIN_TYPE_MSG, ROTWIN_TYPE_CHAT, INTGAME_MODE_DIALOG=9};
''' + 'enum {' + ','.join('OBJ_TYPE_' + t for t in types) + '};\n'
code += re.search(r'typedef struct ModernDetails \{.*?\} ModernDetails;', header, re.S)[0]
code += r'''
static ModernDetails* modern_details_capture;
static bool modern_details_passthrough;
static ModernDetails modern_hover_snapshot;
static int64_t modern_hover_object, hovered;
static int intgame_iso_window_type, mode, target_type=OBJ_TYPE_NPC, examinations, legacy_writes;
static bool enabled=true, valid=true;
static bool modern_ui_enabled(void){return enabled;}
static int64_t object_hover_obj_get(void){return hovered;}
static bool obj_handle_is_valid(int64_t o){(void)o;return valid;}
static int intgame_mode_get(void){return mode;}
static int obj_field_int32_get(int64_t o,int f){(void)o;(void)f;return target_type;}
static void format(int64_t pc,int64_t obj,char* text){
    (void)pc;(void)obj;examinations++;
    if(modern_details_capture){
        snprintf(modern_details_capture->lines[modern_details_capture->count++],2000,"%s",text);
        assert(modern_details_passthrough);
    }
    if(!modern_details_capture || modern_details_passthrough)legacy_writes++;
}
#define intgame_examine_portal format
#define intgame_examine_container format
#define intgame_examine_scenery format
#define intgame_examine_item format
#define intgame_examine_critter format
static void intgame_message_window_display_str(int i,char* text){format(i,0,text);}
''' + function('intgame_examine_object') + '\n' + function('modern_hover_details') + r'''
int main(void){
    hovered=7;intgame_examine_object(1,7,"Original localized name");
    const ModernDetails* d=modern_hover_details();
    assert(d && strcmp(d->title,"Original localized name")==0 && d->count==1);
    assert(examinations==1 && legacy_writes==1);
    assert(!modern_details_capture && !modern_details_passthrough);
    for(int i=0;i<100;i++)assert(modern_hover_details()==d);
    assert(examinations==1); /* Rendering never reruns scripts or examination. */
    hovered=0;assert(!modern_hover_details());
    hovered=8;assert(!modern_hover_details());
    hovered=7;valid=false;assert(!modern_hover_details());valid=true;
    mode=INTGAME_MODE_DIALOG;assert(!modern_hover_details());mode=0;
    target_type=OBJ_TYPE_CONTAINER;hovered=8;intgame_examine_object(1,8,"Locked chest");
    assert(strcmp(modern_hover_details()->title,"Locked chest")==0);
    target_type=OBJ_TYPE_WALL;intgame_examine_object(1,8,"Wall");assert(!modern_hover_details());
    enabled=false;target_type=OBJ_TYPE_NPC;intgame_examine_object(1,8,"Classic NPC");
    assert(examinations==3 && legacy_writes==3 && !modern_hover_details());
    puts("PASS: passive localized capture, preserved classic rendering, target changes and dialogue exclusion");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-hover-test-') as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
