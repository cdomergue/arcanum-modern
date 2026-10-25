#!/usr/bin/env python3
"""Exercise the real AI search with ground items, junk piles and party ownership."""
from pathlib import Path
import os, re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
s = (root/'source/src/game/ai.c').read_text()
function = re.search(r'^bool ai_look_for_item_func\([^\n]*\)\n\{.*?^\}', s, re.M | re.S).group()
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define MODERN_FOLLOWER_AUTO_PICKUP_KEY "modern follower auto pickup"
enum {OBJ_TM_ITEM,OBJ_TM_WEAPON,OBJ_TM_AMMO,OBJ_TM_ARMOR,OBJ_TM_GOLD,OBJ_TM_FOOD,OBJ_TM_SCROLL,OBJ_TM_KEY,OBJ_TM_KEY_RING,OBJ_TM_WRITTEN,OBJ_TM_GENERIC,OBJ_TM_CONTAINER};
enum {OBJ_TYPE_WEAPON,OBJ_TYPE_AMMO,OBJ_TYPE_ARMOR,OBJ_TYPE_GOLD,OBJ_TYPE_FOOD,OBJ_TYPE_SCROLL,OBJ_TYPE_KEY,OBJ_TYPE_KEY_RING,OBJ_TYPE_WRITTEN,OBJ_TYPE_GENERIC};
enum {OBJ_F_ITEM_FLAGS,OBJ_F_FLAGS,STAT_PERCEPTION,BP_JUNK_PILE=42};
#define OIF_NO_DISPLAY 1
#define OIF_NO_NPC_PICKUP 2
#define OF_OFF 4
#define ITEM_CANNOT_OK 0
typedef struct ObjectNode { int64_t obj;struct ObjectNode* next; } ObjectNode;
typedef struct {ObjectNode* head;} ObjectList;
static int settings,enabled=1,scans,pickups,cleanups,item_flags,insert_result;
static bool network,ground=true,junk,monstrous,reserved;
static int64_t leader=1,last_item;
static ObjectNode item={10,0},pile={20,0};
static int64_t critter_pc_leader_get(int64_t obj){assert(obj==2);return leader;}
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static bool tig_net_is_active(void){return network;}
static int settings_get_value(int* s,const char* key){(void)s;(void)key;return enabled;}
static bool critter_is_monstrous(int64_t obj){(void)obj;return monstrous;}
static int stat_level_get(int64_t o,int s){(void)o;(void)s;return 8;}
static int ai_perception_distance(int p){return p;}
static void ai_objects_in_radius(int64_t o,int r,ObjectList* list,unsigned int flags){(void)o;(void)r;scans++;list->head=flags==OBJ_TM_CONTAINER?(junk?&pile:0):(ground?&item:0);}
static int obj_field_int32_get(int64_t o,int field){(void)o;return field==OBJ_F_ITEM_FLAGS?item_flags:0;}
static bool sub_461F60(int64_t o){(void)o;return reserved;}
static int item_check_insert(int64_t a,int64_t b,void* c){(void)a;(void)b;(void)c;return insert_result;}
static int64_t object_dist(int64_t a,int64_t b){(void)a;(void)b;return 1;}
static bool anim_goal_pickup_item(int64_t o,int64_t item){assert(o==2);pickups++;last_item=item;return true;}
static void object_list_destroy(ObjectList* l){(void)l;cleanups++;}
static int sub_49B290(int64_t o){assert(o==20);return BP_JUNK_PILE;}
static int64_t item_find_first_of_type(int64_t o,int type){assert(o==20);(void)type;return 30;}
static int64_t item_find_first(int64_t o){assert(o==20);return 30;}
''' + function + r'''
int main(void){
 for(unsigned int type=OBJ_TM_ITEM;type<=OBJ_TM_GENERIC;type++){
  ground=true;junk=false;enabled=1;
  assert(ai_look_for_item_func(2,type)&&last_item==10);
  enabled=0;int before=scans,orders=pickups;
  assert(!ai_look_for_item_func(2,type)&&scans==before&&pickups==orders);
  ground=false;junk=true;
  assert(!ai_look_for_item_func(2,type)&&scans==before);
  enabled=1;assert(ai_look_for_item_func(2,type)&&last_item==30);
 }
 enabled=0;ground=true;
 leader=0;assert(ai_look_for_item_func(2,OBJ_TM_ITEM)); /* Unrecruited NPC. */
 leader=3;assert(ai_look_for_item_func(2,OBJ_TM_ITEM)); /* Another party. */
 leader=1;network=true;assert(ai_look_for_item_func(2,OBJ_TM_ITEM));network=false;
 assert(!ai_look_for_item_func(2,OBJ_TM_ITEM));
 assert(anim_goal_pickup_item(2,10)); /* Explicit player order stays available. */
 enabled=1;monstrous=true;assert(!ai_look_for_item_func(2,OBJ_TM_ITEM));monstrous=false;
 item_flags=OIF_NO_NPC_PICKUP;assert(!ai_look_for_item_func(2,OBJ_TM_ITEM));item_flags=0;
 reserved=true;assert(!ai_look_for_item_func(2,OBJ_TM_ITEM));reserved=false;
 insert_result=1;assert(!ai_look_for_item_func(2,OBJ_TM_ITEM));insert_result=0;
 assert(ai_look_for_item_func(2,OBJ_TM_ITEM));assert(scans==cleanups);
 puts("PASS: all autonomous item searches, junk piles, toggle, ownership, network isolation and explicit pickup");
}
'''
assert 'settings_register(&settings, MODERN_FOLLOWER_AUTO_PICKUP_KEY, "1", NULL)' in (root/'source/src/game/gamelib.c').read_text()
with tempfile.TemporaryDirectory(prefix='arcanum-pickup-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
