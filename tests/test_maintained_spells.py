#!/usr/bin/env python3
"""Exercise live spell filtering and stop guards, including stale HUD clicks."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1] / 'source/src'
def extract(file,name):
    source=(root/file).read_text()
    m=re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    return source[m.start():source.index('\n}',m.end())+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define SPELL_COUNT 80
#define MAGICTECH_RUN_ACTIVE 1
#define MAGICTECH_ACTION_END 9
#define INTGAME_MODE_DIALOG 8
typedef struct {int flags;struct{int64_t obj;} source_obj;int spell,action,id;} MagicTechRunInfo;
static MagicTechRunInfo runs[512],*magictech_run_info=runs;
static struct Spell {struct {int period;} maintenance;} spells[80],*magictech_spells=spells;
static int64_t actor=1,maintained_pressed_actor=1;
static bool ready=true,hud=true,journal,panel,menu;
static int stopped=-1,maintained_page,maintained_pressed_id=-1,mode;
static int64_t combat_input_actor(void){return actor;}
static bool combat_input_ready(int64_t a){return a==actor && ready;}
static bool native_panel(void){return panel;}
static bool mainmenu_ui_is_active(void){return menu;}
static int intgame_mode_get(void){return mode;}
static bool magictech_id_to_run_info(int id,MagicTechRunInfo** out){
 for(int i=0;i<512;i++){if(runs[i].id==id){*out=&runs[i];return true;}}
 return false;
}
static void magictech_interrupt(int id){stopped=id;}
'''
for file,name in [('game/magictech.c','modern_maintained_spells'),('game/magictech.c','modern_spell_stop'),('ui/modern_ui.c','maintained_action')]:
    code+='\n'+extract(file,name)
code+=r'''
int main(void){
 spells[20].maintenance.period=10;spells[30].maintenance.period=10;
 runs[0]=(MagicTechRunInfo){1,{1},20,0,100};
 runs[1]=(MagicTechRunInfo){1,{2},20,0,101}; /* companion */
 runs[2]=(MagicTechRunInfo){1,{1},30,0,102};
 runs[3]=(MagicTechRunInfo){0,{1},20,0,103}; /* inactive */
 runs[4]=(MagicTechRunInfo){1,{1},20,9,104}; /* ending */
 runs[5]=(MagicTechRunInfo){1,{1},0,0,105}; /* instantaneous */
 runs[6]=(MagicTechRunInfo){1,{1},80,0,106}; /* invalid spell */
 int ids[4],values[4];assert(modern_maintained_spells(1,ids,values,4)==2);
 assert(ids[0]==100&&ids[1]==102&&values[1]==30);
 assert(modern_maintained_spells(1,ids,values,1)==1);
 assert(modern_maintained_spells(2,ids,values,4)==1&&ids[0]==101);
 assert(!modern_spell_stop(1,101));assert(!modern_spell_stop(1,103));
 assert(!modern_spell_stop(1,104));assert(!modern_spell_stop(1,105));
 assert(!modern_spell_stop(1,106));assert(!modern_spell_stop(1,999));
 ready=false;assert(!modern_spell_stop(1,100));ready=true;
 maintained_pressed_id=102;runs[0].flags=0; /* first slot expires while held */
 assert(maintained_action(2501)&&stopped==102&&maintained_pressed_id==-1);
 stopped=-1;maintained_pressed_id=102;actor=2;
 assert(maintained_action(2501)&&stopped==-1); /* actor changed */
 actor=1;mode=8;maintained_pressed_id=102;
 assert(maintained_action(2501)&&stopped==-1); /* dialog opened */
 mode=0;maintained_pressed_id=104;
 assert(maintained_action(2500)&&stopped==-1); /* expired held spell */
 magictech_spells=NULL;assert(!modern_spell_stop(1,102));
 assert(modern_maintained_spells(1,ids,values,4)==0);
 assert(!maintained_action(42));
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-maintained-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: maintained spells, ownership, availability, stale clicks and turn changes')
