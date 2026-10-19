#!/usr/bin/env python3
"""Filtered bags preserve native positions and match localized names safely."""
from pathlib import Path
import subprocess,os,tempfile
root=Path(__file__).resolve().parents[1]/'source/src/ui'
s=(root/'modern_panels.inc').read_text();ui=(root/'modern_ui.c').read_text()
def fn(text,name):
 a=text.rfind('\n',0,text.index(name+'('))+1;b=text.index('\n}',a)+2;return text[a:b]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define GLYPH_FIRST 32
#define GLYPH_COUNT 352
enum {OBJ_F_TYPE,OBJ_TYPE_WEAPON,OBJ_TYPE_ARMOR,OBJ_TYPE_FOOD,OBJ_TYPE_SCROLL,OBJ_TYPE_OTHER};
static int inventory_filter;static char inventory_query[128];
static bool obj_handle_is_valid(int64_t o){return o>=1 && o<=5;}
static int obj_field_int32_get(int64_t o,int f){(void)f;return o;}
static int64_t player_get_local_pc_obj(void){return 1;}
static const char* names[]={"","Épée forgée","Armure","Potion","Parchemin","Clé"};
static void object_examine(int64_t o,int64_t pc,char* b){(void)pc;strcpy(b,names[o]);}
static const char* game_text(const char* s){return s;}
static void SDL_strlcat(char* d,const char* s,size_t n){strncat(d,s,n-strlen(d)-1);}
'''+fn(ui,'codepoint')+fn(ui,'fold')+fn(s,'inventory_match')+fn(s,'inventory_query_edit')+r'''
int main(void){
 assert(!inventory_match(0) && !inventory_match(90));
 inventory_query_edit("epee",false);assert(inventory_match(1) && !inventory_match(2));
 inventory_filter=2;assert(!inventory_match(1));
 inventory_query[0]=0;assert(inventory_match(2));
 inventory_filter=3;assert(inventory_match(3) && inventory_match(4) && !inventory_match(5));
 inventory_filter=4;assert(inventory_match(5));
 inventory_filter=0;inventory_query_edit("Clé",false);assert(inventory_match(5));
 inventory_query_edit("",true);assert(!strcmp(inventory_query,"Cl"));
 puts("PASS: accent/case search, category intersections, invalid objects and UTF-8 backspace");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-filters-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
