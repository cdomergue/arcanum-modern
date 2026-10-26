#!/usr/bin/env python3
"""Transferred companions must not leave renderable handles in party lists."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
def function(path,name):
    source=(root/path).read_text()
    match=re.search(r'^(?:static )?\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    assert match,name
    return source[match.start():source.index('\n}',match.end())+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_CRITTER_FOLLOWER_IDX 1
#define OBJ_F_FLAGS 2
#define OBJ_F_TYPE 3
#define OF_DESTROYED 1
#define OF_EXTINCT 2
#define FOLLOWER_UI_SLOTS 6
#define FOLLOWER_UI_FRAME_ART 0
#define FOLLOWER_UI_POISON_BAR_ART 1
#define FOLLOWER_UI_HEALTH_BAR_ART 2
#define FOLLOWER_UI_FATIGUE_BAR_ART 3
#define STAT_POISON_LEVEL 0
#define FOLLOWER_UI_POISON_LEVEL_THRESHOLD 20
#define FOLLOWER_UI_LOW_HP_THRESHOLD 20
typedef int tig_color_t;
typedef struct {int x,y,width,height;} TigRect;
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct {ObjectNode* head;int num_sectors;} ObjectList;
typedef struct {int64_t obj;} FollowerInfo;
static int64_t party[]={2,0,99,3,4,5,6};
static int flags[7],types[7]={0,1,1,1,1,2,1},dword_5E2F98;
static bool follower_ui_visible=true,in_update;
static int follower_ui_top_index,follower_ui_followers_count=1,refreshes,draws;
static int follower_ui_windows[6],hp=25,hpmax=50,fatigue=12,fatigmax=40,last_health,last_fatigue;
static FollowerInfo follower_ui_followers[6];
/* The real engine considers null a valid but empty handle. */
static bool obj_handle_is_valid(int64_t obj){return obj>=0&&obj<7;}
static int obj_field_int32_get(int64_t obj,int field){assert(obj>0&&obj<7);return field==OBJ_F_FLAGS?flags[obj]:types[obj];}
static bool obj_type_is_critter(int type){return type==1;}
static int obj_arrayfield_length_get(int64_t obj,int field){assert(obj==1&&field==1);return 7;}
static int64_t obj_arrayfield_handle_get(int64_t obj,int field,int i){assert(obj==1&&field==1);return party[i];}
static ObjectNode* object_node_create(void){return calloc(1,sizeof(ObjectNode));}
static int64_t player_get_local_pc_obj(void){return 1;}
static int tig_color_make(int r,int g,int b){return r+g+b;}
/* Null saved object IDs used to leave a stale handle untouched (the crash). */
static bool sub_444130(FollowerInfo* f){(void)f;return true;}
static void follower_ui_refresh(void){refreshes++;assert(in_update);}
static void assert_live(int64_t obj){assert(obj>0&&obj<7&&!flags[obj]&&types[obj]==1);}
static int object_hp_current(int64_t obj){assert_live(obj);return hp;}
static int object_hp_max(int64_t obj){assert_live(obj);return hpmax;}
static int critter_fatigue_current(int64_t obj){assert_live(obj);return fatigue;}
static int critter_fatigue_max(int64_t obj){assert_live(obj);return fatigmax;}
static int stat_level_get(int64_t obj,int stat){assert_live(obj);(void)stat;return 0;}
static bool intgame_examine_portrait(int64_t pc,int64_t obj,int* portrait){assert(pc==1);assert_live(obj);*portrait=1;return true;}
static void portrait_draw_32x32(int64_t obj,int portrait,int win,int x,int y){assert_live(obj);(void)portrait;(void)win;(void)x;(void)y;}
static void tig_window_tint(int win,TigRect* rect,int color,int mode){(void)win;(void)rect;(void)color;(void)mode;}
static void follower_ui_draw(int win,int art,int x,int y,int percent,int scale){
 (void)win;(void)x;(void)y;(void)scale;assert(percent>=0&&percent<=100);draws++;
 if(art==FOLLOWER_UI_HEALTH_BAR_ART)last_health=percent;
 if(art==FOLLOWER_UI_FATIGUE_BAR_ART)last_fatigue=percent;
}
'''
code+=function('source/src/game/object.c','object_list_followers')
for name in ['follower_ui_bar_percent','follower_ui_update','modern_followers_get']:
    code+=function('source/src/ui/follower_ui.c',name)
code+=r'''
int main(void){
 flags[3]=OF_DESTROYED;flags[4]=OF_EXTINCT;
 ObjectList list;object_list_followers(1,&list);assert(dword_5E2F98==1);
 assert(list.head->obj==2&&list.head->next->obj==6&&!list.head->next->next);
 while(list.head){ObjectNode* n=list.head;list.head=n->next;free(n);}
 int64_t output[8]={0};assert(modern_followers_get(output,8)==2&&output[0]==2&&output[1]==6);
 assert(party[3]==3&&party[4]==4); /* Keep saved identities for destination lookup. */
 assert(modern_followers_get(output,1)==1);
 const int invalid[]={0,99,3,4,5};
 for(int i=0;i<5;i++){
  follower_ui_followers[0].obj=invalid[i];int before=refreshes;draws=0;
  follower_ui_update();assert(refreshes==before+1&&!draws&&!in_update);
 }
 follower_ui_followers[0].obj=2;follower_ui_update();assert(last_health==50&&last_fatigue==30);
 hpmax=fatigmax=0;follower_ui_update();assert(!last_health&&!last_fatigue);
 hpmax=fatigmax=-1;follower_ui_update();assert(!last_health&&!last_fatigue);
 hpmax=fatigmax=10;hp=fatigue=-10;follower_ui_update();assert(!last_health&&!last_fatigue);
 hp=fatigue=INT_MAX;follower_ui_update();assert(last_health==100&&last_fatigue==100);
 assert(follower_ui_bar_percent(INT_MAX/2,INT_MAX)==49);
 puts("PASS: unloaded/invalid/non-critter followers excluded without losing saved references; stale cache refresh and bounded HP/fatigue bars");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-follower-lifetime-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
