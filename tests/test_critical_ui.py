#!/usr/bin/env python3
"""Exercise real party hit regions and item spell selection with engine fixtures."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
def function(src,name):
    start=src.index(name);return src[start:src.index('\n}',start)+2]
party=(root/'source/src/ui/modern_party.inc').read_text()
combat=(root/'source/src/ui/modern_combat.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
typedef struct {float x,y,w,h;} SDL_FRect;
static int canvas_h=1080,party_count,party_page,party_menu_obj;
static bool selected;
static bool party_visible(void){return true;}
static int modern_party_selected(void){return selected;}
static SDL_FRect party_menu_rect(void){return (SDL_FRect){114,88,265,329};}
static bool inside(SDL_FRect r,float x,float y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
'''
code+=re.search(r'static int party_rows\(void\)\{[^\n]+',party).group()+'\n'
code+=function(party,'static bool party_region(')
code+=r'''
#define SPELL_COUNT 80
static int tactical_spell_page,selector=1,tactical_maintained_count,tactical_maintained_ids[32];
static int64_t active=1,cast_source;
static int cast_spell=-1,known_count;
static bool combat,ready=true;
static int ap=4;
typedef struct {int64_t owner; bool identified; int complexity; int spells[5];} Item;
static Item inventory[4];
static bool obj_handle_is_valid(int64_t h){return h>=10&&h<14;}
static bool item_parent(int64_t h,int64_t* owner){*owner=inventory[h-10].owner;return *owner!=0;}
static bool item_is_identified(int64_t h){return inventory[h-10].identified;}
static int item_magic_tech_complexity(int64_t h){return inventory[h-10].complexity;}
static bool magictech_is_tech(int s){return s==100;}
static bool sub_45A030(int s){assert(s>=0&&s!=10000);return s==101;}
static int mt_item_spell(int64_t h,int slot){return inventory[h-10].spells[slot];}
static bool spell_is_known(int64_t actor,int s){return actor==1&&s<known_count;}
static int item_list_get(int64_t actor,int64_t** out){static int64_t list[4];int n=0;for(int i=0;i<4;i++)if(inventory[i].owner==actor)list[n++]=10+i;*out=list;return n;}
static void item_list_free(int64_t* p){(void)p;}
static bool combat_turn_based_is_active(void){return combat;}
static int combat_action_points_get(void){return ap;}
static bool player_is_local_pc_obj(int64_t h){return h==1;}
static int critter_fatigue_current(int64_t h){(void)h;return 100;}
static int64_t combat_input_actor(void){return active;}
static bool combat_input_ready(int64_t h){(void)h;return ready;}
static void modern_spell_stop(int64_t h,int id){(void)h;(void)id;}
static bool modern_spell_resources(int64_t h,int spell,int* cost){*cost=5;return spell_is_known(h,spell);}
static void spell_ui_activate(int64_t source,int spell){cast_source=source;cast_spell=spell;}
'''
start=combat.index('typedef struct SelectorSpell');end=combat.index('static bool tactical_action',start)
code+=combat[start:end]
code+=r'''
int main(void){
 for(int h=720;h<=1440;h+=180){canvas_h=h;int rows=party_rows();
  for(int n=rows+1;n<=128;n++){party_count=n;party_page=(n-1)/rows;
   assert(party_region(20,88+rows*108+12));assert(party_region(80,88+rows*108+12));
   selected=true;assert(party_region(40,88+rows*108+28+14));selected=false;
  }
 }
 for(int i=0;i<4;i++){inventory[i].owner=1;inventory[i].identified=true;inventory[i].complexity=10;for(int j=0;j<5;j++)inventory[i].spells[j]=-1;}
 inventory[0].spells[0]=42;inventory[0].spells[1]=43;
 inventory[1].spells[0]=44;inventory[1].identified=false;
 inventory[2].spells[0]=100;inventory[2].spells[1]=101;
 inventory[3].spells[0]=45;inventory[3].complexity=-5;
 selector_spells_collect(1);assert(selector_spell_count==2);
 assert(selector_spells[0].item==10&&selector_spells[0].spell==42);
 selector_spell_action(1540);assert(cast_source==10&&cast_spell==42);
 selector=1;cast_source=0;inventory[0].owner=2;selector_spell_action(1540);assert(!cast_source);
 inventory[0].owner=1;combat=true;ready=false;selector_spell_action(1540);assert(!cast_source);
 ready=true;active=2;selector_spell_action(1540);assert(!cast_source);
 active=1;combat=false;known_count=80;selector_spells_collect(1);assert(selector_spell_pages==9);
 tactical_spell_page=7;selector_spell_action(1111);assert(tactical_spell_page==8);
 selector_spells_collect(1);assert(selector_spell_count==2&&selector_spells[0].item==10);
 selector_spell_action(1111);assert(tactical_spell_page==8);
 known_count=0;inventory[0].owner=2;selector_spells_collect(1);assert(tactical_spell_page==0&&selector_spell_count==0);
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'check.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'check.c'),'-lm','-o',str(p/'check')],check=True)
    subprocess.run([str(p/'check')],check=True)
print('PASS: partial party pages, footer, item spell sources, identification, passive effects, transfers, actor changes and ninth spell page')
