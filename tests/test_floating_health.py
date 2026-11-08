#!/usr/bin/env python3
"""Run the production HP setter, bounded feedback queue and animation renderer."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/game/object.c').read_text();start=s.index('int object_hp_damage_set(int64_t obj, int value)\n{')
setter=s[start:s.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
typedef uint64_t Uint64;
typedef struct {int64_t id;} ObjectID;
typedef struct {int x,y,width,height;} TigRect;
typedef struct {unsigned char r,g,b,a;} SDL_Color;
enum {OBJ_F_TYPE,OBJ_F_FLAGS,OBJ_F_SPELL_FLAGS,OBJ_F_HP_DAMAGE,OBJ_F_LOCATION};
enum {OF_OFF=1,OF_DESTROYED=2,OF_EXTINCT=4,OF_DONTDRAW=8,OF_INVISIBLE=16,OSF_INVISIBLE=32};
static bool enabled=true,in_game=true,loading,resetting,menu,movie,journal,panel;
static Uint64 now=1000;
static bool covered;
static bool roof_is_covered_loc(int64_t loc,bool faded){(void)loc;assert(faded);return covered;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)field;return obj;}
static int object_hp_max(int64_t obj){(void)obj;return 34;}
static int flags[90],spells[90],damage[90],types[90],width=1280,height=720,camera_x;
static int identities[90];
static float canvas_w=1280,canvas_h=720;
static struct {float size;} fonts[3]={{20},{20},{24}};
static char drawn[32];static float drawn_x,drawn_y;static SDL_Color drawn_color;static int draws;
static Uint64 SDL_GetTicks(void){return now;}
static bool gamelib_in_load(void){return loading;}
static bool gamelib_in_reset(void){return resetting;}
static bool mainmenu_ui_is_active(void){return menu;}
static bool tig_movie_is_playing(void){return movie;}
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<90&&identities[obj]!=0;}
static int obj_field_int32_get(int64_t obj,int field){return field==OBJ_F_TYPE?types[obj]:field==OBJ_F_FLAGS?flags[obj]:field==OBJ_F_SPELL_FLAGS?spells[obj]:damage[obj];}
static void obj_field_int32_set(int64_t obj,int field,int val){assert(field==OBJ_F_HP_DAMAGE);damage[obj]=val;}
static bool obj_type_is_critter(int type){return type==1;}
static int64_t player_get_local_pc_obj(void){return 1;}
static bool critter_is_concealed(int64_t obj){return obj==89;}
static int64_t critter_pc_leader_get(int64_t obj){(void)obj;return 0;}
static void object_get_rect(int64_t obj,unsigned flags,TigRect* r){(void)obj;(void)flags;*r=(TigRect){500-camera_x,300,50,80};}
static int hrp_iso_window_width_get(void){return width;}
static int hrp_iso_window_height_get(void){return height;}
static ObjectID obj_get_id(int64_t obj){return (ObjectID){identities[obj]};}
static bool objid_is_equal(ObjectID a,ObjectID b){return a.id==b.id;}
static bool visible(void){return enabled&&in_game&&!menu;}
static int native_panel(void){return panel;}
static float text_measure(int font,const char* text){(void)font;return strlen(text)*12;}
static void text_draw(int font,const char* text,float x,float y,SDL_Color color){(void)font;snprintf(drawn,sizeof(drawn),"%s",text);drawn_x=x;drawn_y=y;drawn_color=color;draws++;}
static void critter_resting_timeevent_schedule(int64_t obj){(void)obj;}
static void ui_refresh_health_bar(int64_t obj){(void)obj;}
static int object_hp_damage_get(int64_t obj){return damage[obj];}
''' +(root/'source/src/ui/modern_floating_health.inc').read_text()+'\n'+setter+r'''
static int count(void){int n=0;for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++)if(health_floats[i].delta)n++;return n;}
static void fresh(void){modern_health_reset();damage[1]=0;now+=2000;draws=0;}
int main(void){
 for(int i=1;i<90;i++){identities[i]=i;types[i]=1;}
 object_hp_damage_set(1,12);assert(damage[1]==12&&count()==1);draw_floating_health();
 assert(!strcmp(drawn,"-12")&&drawn_color.r>drawn_color.g&&drawn_color.a==255);
 float y=drawn_y,x=drawn_x;now+=600;draw_floating_health();assert(drawn_y<y&&drawn_color.a<255);
 camera_x=100;draw_floating_health();assert(drawn_x==x-100);camera_x=0;
 now+=600;draws=0;draw_floating_health();assert(!count()&&!draws);
 object_hp_damage_set(1,3);draw_floating_health();assert(!strcmp(drawn,"+9")&&drawn_color.g>drawn_color.r);
 fresh();damage[1]=3;object_hp_damage_set(1,-99);draw_floating_health();assert(damage[1]==0&&!strcmp(drawn,"+3"));
 object_hp_damage_set(1,0);assert(count()==1);
 fresh();for(int i=0;i<10;i++){now++;object_hp_damage_set(1,i+1);}assert(count()==HEALTH_FLOAT_LANES);
 fresh();for(int i=1;i<85;i++){now++;modern_health_change(i,0,1);}assert(count()==HEALTH_FLOAT_CAPACITY);
 fresh();flags[1]=OF_DONTDRAW;object_hp_damage_set(1,1);assert(!count());flags[1]=0;
 spells[1]=OSF_INVISIBLE;modern_health_change(1,0,1);assert(!count());spells[1]=0;
 modern_health_change(89,0,1);assert(!count());
 types[1]=0;modern_health_change(1,0,1);assert(!count());types[1]=1;
 camera_x=2000;modern_health_change(1,0,1);assert(!count());camera_x=0;
 bool* gates[]={&loading,&resetting,&menu,&movie};
 for(int i=0;i<4;i++){*gates[i]=true;modern_health_change(1,0,1);assert(!count());*gates[i]=false;}
 enabled=false;modern_health_change(1,0,1);assert(!count());enabled=true;
 modern_health_change(1,0,1);identities[1]=99;draw_floating_health();assert(!count());identities[1]=1;
 modern_health_change(1,0,1);identities[1]=0;draw_floating_health();assert(!count());identities[1]=1;
 fresh();damage[1]=20;object_hp_damage_set(1,40);assert(count()==1&&health_floats[0].delta==-14);
 object_hp_damage_set(1,32000);assert(count()==1);
 fresh();damage[1]=32000;object_hp_damage_set(1,24);assert(count()==1&&health_floats[0].delta==10);
 fresh();covered=true;object_hp_damage_set(1,5);assert(!count());covered=false;
 modern_health_change(1,0,5);covered=true;draw_floating_health();assert(!count());covered=false;
 fresh();modern_health_change(1,0,INT_MAX);draw_floating_health();assert(!strcmp(drawn,"-34"));
 fresh();modern_health_change(1,0,5);width=640;height=360;draw_floating_health();assert(drawn_x>900);width=1280;height=720;
 panel=true;now+=HEALTH_FLOAT_LIFETIME;draw_floating_health();panel=false;draws=0;draw_floating_health();assert(!count()&&!draws);
 modern_health_change(1,0,1);modern_health_reset();assert(!count());
 puts("PASS: production HP damage/heal/clamp, color, rise/fade/expiry, camera/zoom, bounded bursts, hidden/stale actors and load/reset guards");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-health-floats-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
