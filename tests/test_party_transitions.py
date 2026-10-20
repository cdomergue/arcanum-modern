#!/usr/bin/env python3
"""Exercise recursive transfer scheduling with followers on different maps."""
from pathlib import Path
import os, re, subprocess, tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'source/src/game/teleport.c').read_text()
function = re.search(r'^bool schedule_teleport_obj_recursively\([^\n]+\)\n\{.*?^\}', source, re.M|re.S).group()
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define OF_DESTROYED 1
#define OF_EXTINCT 2
#define OF_TEXT 4
#define OF_TEXT_FLOATER 8
#define ONF_AI_WAIT_HERE 1
#define OBJ_TYPE_PC 1
#define OBJ_TYPE_NPC 2
#define OBJ_TYPE_CONTAINER 3
#define OBJ_F_TYPE 1
#define OBJ_F_FLAGS 2
#define OBJ_F_NPC_FLAGS 3
#define OBJ_F_CRITTER_INVENTORY_NUM 4
#define OBJ_F_CRITTER_INVENTORY_LIST_IDX 5
#define OBJ_F_CONTAINER_INVENTORY_NUM 6
#define OBJ_F_CONTAINER_INVENTORY_LIST_IDX 7
#define OBJ_F_LOCATION 8
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct {ObjectNode* head;} ObjectList;
typedef struct TeleportObjectNode {int64_t loc,obj;struct TeleportObjectNode* next;} TeleportObjectNode;
static TeleportObjectNode* teleport_obj_node_head;
static bool teleport_is_teleporting_pc;
static int flags[5],waiting;
static ObjectNode followers[]={{2,&followers[1]},{3,NULL}};
static bool obj_handle_is_valid(int64_t obj){return obj>0&&obj<5;}
static int obj_field_int32_get(int64_t obj,int field){
 assert(obj_handle_is_valid(obj));
 if(field==OBJ_F_TYPE)return obj==1?OBJ_TYPE_PC:obj<4?OBJ_TYPE_NPC:4;
 if(field==OBJ_F_FLAGS)return flags[obj];
 if(field==OBJ_F_NPC_FLAGS)return obj==3?waiting:0;
 if(field==OBJ_F_CRITTER_INVENTORY_NUM)return obj==2?1:0;
 return 0;
}
static bool obj_type_is_critter(int type){return type==OBJ_TYPE_PC||type==OBJ_TYPE_NPC;}
static void object_list_followers(int64_t obj,ObjectList* list){list->head=obj==1?followers:NULL;}
static void object_list_destroy(ObjectList* list){(void)list;}
static int obj_arrayfield_length_get(int64_t obj,int field){(void)field;return obj==2?1:0;}
static int64_t obj_arrayfield_handle_get(int64_t obj,int field,int index){(void)obj;(void)field;(void)index;return 4;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)obj;(void)field;return 42;}
static void tb_remove(int64_t obj){(void)obj;}
static void tf_remove(int64_t obj){(void)obj;}
static TeleportObjectNode* teleport_obj_node_create(void){return calloc(1,sizeof(TeleportObjectNode));}
#define tig_debug_printf(...) ((void)0)
''' + function + r'''
static int scheduled(void){
 int mask=0;
 while(teleport_obj_node_head){
  TeleportObjectNode* node=teleport_obj_node_head;
  assert(!(mask&(1<<node->obj)));mask|=1<<node->obj;
  teleport_obj_node_head=node->next;free(node);
 }
 return mask;
}
int main(void){
 assert(schedule_teleport_obj_recursively(1,42));
 assert(scheduled()==30&&teleport_is_teleporting_pc);
 for(int state=1;state<=3;state++){
  flags[2]=state;teleport_is_teleporting_pc=false;
  assert(schedule_teleport_obj_recursively(1,42));
  assert(scheduled()==10&&teleport_is_teleporting_pc);
 }
 flags[2]=0;waiting=ONF_AI_WAIT_HERE;
 assert(schedule_teleport_obj_recursively(1,42));assert(scheduled()==22);
 assert(schedule_teleport_obj_recursively(0,42));assert(!scheduled());
 assert(schedule_teleport_obj_recursively(999,42));assert(!scheduled());
 puts("PASS: live followers and inventory transfer; absent/destroyed/extinct followers and waiting allies stay excluded");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-transitions-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
