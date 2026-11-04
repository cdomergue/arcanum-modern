"""Compile the real item-spell entry point and check consumable routing."""
from pathlib import Path
import os
import subprocess
import tempfile
source = (Path(__file__).resolve().parents[1] / 'source/src/ui/modern_hotkeys.inc').read_text()
start = source.index('void modern_item_spell_activate(')
function = source[start:source.index('\n}', start)+2]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum { OBJ_F_TYPE, OBJ_TYPE_SCROLL=10 };
static bool ready=true,valid=true;
static int type=OBJ_TYPE_SCROLL,items,spells;
static bool combat_input_ready(int64_t actor){return ready;}
static bool hotkey_item_spell_valid(int64_t actor,int64_t item,int spell){return valid;}
static int obj_field_int32_get(int64_t item,int field){return type;}
static void item_ui_activate(int64_t actor,int64_t item){assert(actor==1 && item==2);items++;}
static void spell_ui_activate(int64_t item,int spell){assert(item==2 && spell==3);spells++;}
'''
checks = r'''
int main(void){
    ready=false;modern_item_spell_activate(1,2,3);assert(!items && !spells);
    ready=true;valid=false;modern_item_spell_activate(1,2,3);assert(!items && !spells);
    valid=true;modern_item_spell_activate(1,2,3);assert(items==1 && !spells);
    type=20;modern_item_spell_activate(1,2,3);assert(items==1 && spells==1);
    puts("Scroll native item targeting, reusable item spell, invalid source and turn guards: OK");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path=Path(directory)
    (path/'test.c').write_text(harness+function+checks)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
