#!/usr/bin/env python3
"""Exercise passive hover capture without repeating examination side effects."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/intgame.c').read_text() + '\n' + (root / 'source/src/ui/tb_ui.c').read_text()
header = (root / 'source/src/ui/modern_ui.h').read_text()

def function(name):
    match = re.search(r'^(?:void|bool|const ModernDetails\*) ' + name + r'\([^\n]*\)\n\{', source, re.M)
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
#define MAX_STRING 2000
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
static bool enabled=true, valid=true, scripted;
static int script_calls;
static const char* object_name;
static void object_examine(int64_t obj,int64_t pc,char* out){(void)obj;(void)pc;strcpy(out,object_name);}
static bool modern_ui_enabled(void){return enabled;}
static int64_t object_hover_obj_get(void){return hovered;}
static bool obj_handle_is_valid(int64_t o){(void)o;return valid;}
static int intgame_mode_get(void){return mode;}
static int obj_field_int32_get(int64_t o,int f){(void)o;(void)f;return target_type;}
static void format(int64_t pc,int64_t obj,char* text){
    (void)pc;(void)obj;examinations++;
    if(modern_details_capture){
        snprintf(modern_details_capture->lines[modern_details_capture->count++],2000,"%s",text);

    }
    if(!modern_details_capture || modern_details_passthrough)legacy_writes++;
}
#define intgame_examine_portal format
#define intgame_examine_container format
#define intgame_examine_scenery format
#define intgame_examine_item format
#define intgame_examine_critter format
static void intgame_message_window_display_str(int i,char* text){format(i,0,text);}
static void intgame_message_window_display_attack(int64_t obj){format(1,obj,"Attack");}
static void intgame_message_window_display_defense(int64_t obj){format(1,obj,"Defense");}
static bool sub_57CD60(int64_t pc,int64_t obj,char* out){
    script_calls++;
    if(scripted){format(pc,obj,"Document établi au nom de Test");return false;}
    object_examine(obj,pc,out);return true;
}
static void sub_557370(int64_t pc,int64_t obj){(void)pc;(void)obj;}
''' + '\n'.join(function(n) for n in ['modern_examine_begin','modern_examine_end','intgame_examine_object','sub_57CCF0','modern_inventory_details','modern_hover_details']) + r'''
int main(void){
    hovered=7;object_name="Original localized name";sub_57CCF0(1,7);
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
    target_type=OBJ_TYPE_CONTAINER;hovered=8;object_name="Locked chest";sub_57CCF0(1,8);
    assert(strcmp(modern_hover_details()->title,"Locked chest")==0);
    target_type=OBJ_TYPE_WALL;object_name="Wall";sub_57CCF0(1,8);assert(!modern_hover_details());
    enabled=false;target_type=OBJ_TYPE_NPC;object_name="Classic NPC";sub_57CCF0(1,8);
    assert(examinations==3 && legacy_writes==3 && !modern_hover_details());
    enabled=true;scripted=true;target_type=OBJ_TYPE_WRITTEN;object_name="Document";hovered=9;
    int before=script_calls;sub_57CCF0(1,9);assert(script_calls==before+1);
    ModernDetails detail;
    for(int i=0;i<100;i++) {
        modern_inventory_details(1,9,0,&detail);
        assert(detail.count==2 && !strcmp(detail.lines[0],"Document"));
        assert(!strcmp(detail.lines[1],"Document établi au nom de Test"));
    }
    assert(script_calls==before+1);
    modern_inventory_details(1,10,0,&detail);assert(detail.count==1);
    scripted=false;sub_57CCF0(1,10);modern_inventory_details(1,10,0,&detail);assert(detail.count==1);
    puts("PASS: native script capture, retained inventory description, no repeated scripts or duplicate fields, classic rendering and dialogue exclusion");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-hover-test-') as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
