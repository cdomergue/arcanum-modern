#!/usr/bin/env python3
"""Exercise production shared-turn selection, budgets, boundaries and save state."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/game/modern_shared_turn.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define OID_TYPE_NULL 0
#define AG_ANIM_FIDGET 1
typedef FILE TigFile;
typedef struct {int type;int64_t id;} ObjectID;
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct {int value;} AnimID;
static ObjectNode nodes[7];
static struct {ObjectNode* head;} combat_critter_list;
static ObjectNode* dword_5FC240;
static bool tactical_enabled=true,combat_turn_based_active=true,loading,blocked;
static int combat_action_points,combat_required_action_points,combat_turn_based_turn=1;
static int64_t qword_5FC258;
static int dword_5FC260;
static bool dword_5FC264;
static bool active[8],controlled[8],busy[8],fidget[8];
static int starts[8];
static bool obj_handle_is_valid(int64_t obj){return obj>0 && obj<8;}
static bool critter_is_active(int64_t obj){return active[obj];}
static bool combat_player_controls(int64_t obj){return obj_handle_is_valid(obj) && controlled[obj];}
static bool sub_4B7DC0(int64_t obj){(void)obj;return false;}
static bool gamelib_in_load(void){return loading;}
static int64_t combat_input_actor(void){return dword_5FC240 && combat_player_controls(dword_5FC240->obj)?dword_5FC240->obj:0;}
static bool combat_input_ready(int64_t obj){return obj && obj==combat_input_actor() && !blocked && !busy[obj];}
static bool sub_423300(int64_t obj,AnimID* id){id->value=1;return busy[obj];}
static bool anim_is_current_goal_type(int64_t obj,int goal,AnimID* id){(void)goal;id->value=1;return fidget[obj];}
static bool anim_id_is_equal(AnimID* a,AnimID* b){return a->value==b->value;}
static TigFile* tig_file_fopen(const char* name,const char* mode){(void)name;return fopen("turns.dat",mode);}
#define tig_file_fread fread
#define tig_file_fwrite fwrite
#define tig_file_fclose fclose
static ObjectID obj_get_id(int64_t actor){return (ObjectID){1,actor};}
static int64_t obj_pool_perm_lookup(ObjectID oid){return oid.type && obj_handle_is_valid(oid.id)?oid.id:0;}
static void combat_turn_based_whos_turn_set(int64_t actor);
static void combat_turn_based_subturn_start(void);
''' + source + r'''
static void combat_turn_based_whos_turn_set(int64_t actor){
    dword_5FC240=combat_critter_list.head;
    while(dword_5FC240 && dword_5FC240->obj!=actor)dword_5FC240=dword_5FC240->next;
    assert(dword_5FC240);
    if(!shared_turn_restore(actor)){combat_action_points=10;shared_turn_started(actor);starts[actor]++;}
}
static void combat_turn_based_subturn_start(void){combat_turn_based_whos_turn_set(dword_5FC240->obj);}
static void reset(void){
    shared_turn_count=0;memset(starts,0,sizeof(starts));
    memset(busy,0,sizeof(busy));memset(fidget,0,sizeof(fidget));
    loading=blocked=false;tactical_enabled=combat_turn_based_active=true;
    for(int i=1;i<8;i++){active[i]=true;controlled[i]=i!=3;nodes[i-1]=(ObjectNode){i,i<7?&nodes[i]:NULL};}
    combat_critter_list.head=nodes;combat_turn_based_whos_turn_set(1);
}
int main(void){
    reset();assert(combat_shared_turn_available(1)&&combat_shared_turn_available(2));
    assert(!combat_shared_turn_available(3)&&!combat_shared_turn_available(4));
    combat_action_points=6;assert(combat_shared_turn_select(2)&&combat_action_points==10);
    combat_action_points=3;assert(combat_shared_turn_select(1)&&combat_action_points==6);
    assert(combat_shared_turn_select(2)&&combat_action_points==3);
    assert(starts[1]==1&&starts[2]==1);
    assert(!combat_shared_turn_select(4)&&!combat_shared_turn_select(3));
    blocked=true;assert(!combat_shared_turn_select(1));blocked=false;
    busy[1]=true;assert(!combat_shared_turn_select(1));fidget[1]=true;
    assert(combat_shared_turn_select(1));busy[1]=fidget[1]=false;
    combat_action_points=0;assert(!combat_shared_turn_select(2));
    assert(shared_turn_next()&&combat_input_actor()==2&&combat_action_points==3);
    assert(!combat_shared_turn_available(1));
    assert(!shared_turn_next()&&dword_5FC240->obj==2);
    combat_turn_based_whos_turn_set(3);assert(!combat_shared_turn_available(4));
    reset();combat_action_points=4;assert(combat_shared_turn_select(2));combat_action_points=7;
    assert(shared_turn_save());shared_turn_count=0;
    nodes[0].next=&nodes[2];nodes[1].next=&nodes[0];combat_critter_list.head=&nodes[1];
    assert(shared_turn_load());assert(combat_critter_list.head==&nodes[0] && nodes[0].next==&nodes[1]);
    assert(combat_shared_turn_select(1)&&combat_action_points==4);
    assert(combat_shared_turn_select(2)&&combat_action_points==7);
    assert(shared_turn_next());assert(shared_turn_save());
    shared_turn_count=0;assert(shared_turn_load());assert(!combat_shared_turn_available(2));
    assert(!shared_turn_next()&&dword_5FC240->obj==2);
    reset();assert(combat_shared_turn_select(2));remove("turns.dat");shared_turn_count=0;
    assert(shared_turn_load());assert(!combat_shared_turn_available(1));
    reset();active[3]=false;assert(combat_shared_turn_available(4));
    assert(combat_shared_turn_select(4));controlled[2]=false;assert(!combat_shared_turn_available(1));
    reset();assert(combat_shared_turn_select(2));active[1]=false;
    assert(!shared_turn_next()&&dword_5FC240->obj==2);
    reset();tactical_enabled=false;assert(!combat_shared_turn_select(2));
    reset();assert(shared_turn_save());FILE* file=fopen("turns.dat","wb");fputs("bad",file);fclose(file);
    assert(!shared_turn_load()&&shared_turn_count==0);
    reset();loading=true;assert(!shared_turn_restore(1));loading=false;
    shared_turn_count=0;combat_turn_based_whos_turn_set(1);assert(combat_action_points==10&&starts[1]==2);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-shared-turns-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],cwd=path,check=True)
print('PASS: shared turns preserve AP, enemy boundaries, ended turns, busy guards and saved state')
