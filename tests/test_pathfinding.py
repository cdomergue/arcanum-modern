#!/usr/bin/env python3
"""Exercise native path entry points and bounded A* against deterministic terrain."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/game/path.c').read_text()
def fn(signature):
 start=s.index(signature+'\n{');return s[start:s.index('\n}',start)+2]+'\n'
h=(root/'source/src/game/path.h').read_text();flags=h[h.index('typedef unsigned int PathFlags;'):h.index('typedef struct WmapPathInfo')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
typedef uint64_t Uint64;
typedef unsigned int tig_timestamp_t;
typedef int tig_duration_t;
#define LOCATION_GET_X(l) ((int64_t)(uint32_t)(l))
#define LOCATION_GET_Y(l) ((int64_t)(uint32_t)((uint64_t)(l)>>32))
static int64_t location_make(int64_t x,int64_t y){return (int64_t)(((uint64_t)y<<32)|(uint32_t)x);}
static unsigned clock_ms=1000,timer_step;
static void tig_timer_now(tig_timestamp_t* t){*t=clock_ms;}
static int tig_timer_elapsed(tig_timestamp_t t){clock_ms+=timer_step;return clock_ms-t;}
static Uint64 SDL_GetTicks(void){return clock_ms;}
static int wall,gap=80,trap_mode,occupied,door,window_mode;
static bool blocked(int64_t loc){int x=LOCATION_GET_X(loc),y=LOCATION_GET_Y(loc);return wall && x==130 && y!=gap;}
static bool tile_is_blocking(int64_t loc,bool loose){(void)loose;return blocked(loc);}
static int64_t location_get_x(int64_t loc){return LOCATION_GET_X(loc);}
static int64_t location_get_y(int64_t loc){return LOCATION_GET_Y(loc);}
static int64_t location_limit_x=512,location_limit_y=512;
enum {OBJ_F_TYPE,OBJ_F_FLAGS,OBJ_TYPE_PC,OBJ_TYPE_NPC,OF_NO_BLOCK=1};
typedef struct ObjectNode{int64_t obj;struct ObjectNode* next;}ObjectNode;
typedef struct{ObjectNode* head;}ObjectList;
#define OBJ_TRAVERSAL_PORTAL_BLOCKS 1
#define OBJ_TRAVERSAL_WINDOW_BLOCKS 2
#define OBJ_TRAVERSAL_IGNORE_CRITTERS 4
#define OBJ_TM_CRITTER 1
#define OBJ_TM_SCENERY 2
#define OBJ_TM_CONTAINER 4
#define OBJ_HANDLE_NULL 0
static int path_limit=10,path_time_limit=250,dword_5DE5FC,dword_5DE600;
static tig_timestamp_t dword_5DE5F8;
static int64_t selected=2;
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static int64_t modern_party_selected(void){return selected;}
static int obj_field_int32_get(int64_t obj,int field){return field==OBJ_F_TYPE?(obj==1?OBJ_TYPE_PC:OBJ_TYPE_NPC):0;}
static bool obj_type_is_critter(int type){return type==OBJ_TYPE_PC || type==OBJ_TYPE_NPC;}
static bool critter_is_dead(int64_t obj){(void)obj;return false;}
static ObjectNode obstacle={9,NULL};
static void object_list_location(int64_t loc,unsigned f,ObjectList* list){(void)f;list->head=occupied && LOCATION_GET_X(loc)==180?&obstacle:NULL;}
static void object_list_destroy(ObjectList* l){(void)l;}
static bool trap_party_avoids_location(int64_t obj,int64_t loc){(void)obj;return trap_mode==2 && LOCATION_GET_X(loc)==140 && LOCATION_GET_Y(loc)==100;}
static int64_t get_trap_at_location(int64_t loc){return trap_mode && LOCATION_GET_X(loc)==140 && LOCATION_GET_Y(loc)==100?9:0;}
static bool trap_is_spotted(int64_t obj,int64_t trap){(void)obj;(void)trap;return true;}
static int64_t get_fire_at_location(int64_t loc){(void)loc;return 0;}
static uint8_t sub_4D9240(int64_t loc,int x,int y){(void)loc;(void)x;(void)y;return 100;}
''' + flags + r'''
static unsigned int path_translate_flags(PathFlags flags);
static int sub_41F6C0(PathCreateInfo* p){(void)p;return 0;}
static int sub_41F840(PathCreateInfo* p);
static int sub_41F9F0(PathCreateInfo* p);
static int sub_420E30(PathCreateInfo* p,tig_duration_t ms);
static int sub_420660(int64_t a,int64_t b,int8_t* r){(void)a;(void)b;(void)r;return 0;}
static int path_make_straight(int64_t a,int64_t b,int8_t* r){return sub_420660(a,b,r);}
'''
loc=(root/'source/src/game/location.c').read_text()
for sig in ['int location_rot(int64_t a, int64_t b)','bool location_in_dir(int64_t loc, int dir, int64_t* new_loc_ptr)']:
 start=loc.index(sig+'\n{');code+=loc[start:loc.index('\n}',start)+2]+'\n'
code+=r'''
static bool object_calc_traversal_cost(int64_t obj,int64_t loc,int rot,unsigned f,int64_t* block,int* type,bool* window){
 (void)obj;(void)type;int64_t next;assert(location_in_dir(loc,rot,&next));
 bool at_door=door&&LOCATION_GET_X(next)==140;
 if(window)*window=window_mode&&LOCATION_GET_X(next)==140;
 if(block)*block=0;
 return blocked(next)||(at_door&&(f&1))||(window_mode&&LOCATION_GET_X(next)==140&&(f&2));
}
static bool object_traversal_is_blocked(int64_t obj,int64_t loc,int rot,unsigned f,bool* window){int64_t b;int t;return object_calc_traversal_cost(obj,loc,rot,f,&b,&t,window);}
'''
# Header already declares the public translation function, so keep its linkage public.
code=code.replace('static unsigned int path_translate_flags(PathFlags flags);','unsigned int path_translate_flags(PathFlags flags);')
code+=(root/'source/src/game/modern_path_search.inc').read_text()+'\n'
for sig in ['static int path_make_internal(PathCreateInfo* path_create_info)','int path_make(PathCreateInfo* info)','unsigned int path_translate_flags(PathFlags flags)','int sub_41F840(PathCreateInfo* path_create_info)','int sub_41F9F0(PathCreateInfo* path_create_info)','int sub_420E30(PathCreateInfo* path_create_info, tig_duration_t ms)']:
 code+=fn(sig)
code+=r'''
static PathCreateInfo query(int64_t actor,int x,int y,int capacity,int8_t* output){return (PathCreateInfo){actor,location_make(100,100),location_make(x,y),capacity,output,0,0};}
static int full(PathCreateInfo* p){path_trace=(PathTrace){0};return modern_path_search(p,50);}
static void verify(PathCreateInfo* p,int n){int64_t at=p->from;for(int i=0;i<n;i++){assert(p->rotations[i]>=0&&p->rotations[i]<8);assert(location_in_dir(at,p->rotations[i],&at));assert(!blocked(at));}assert(at==p->to);}
int main(void){
 struct{int8_t before,route[200],after;} buffer={77,{0},88};
 PathCreateInfo p=query(1,180,100,200,buffer.route);assert(path_make(&p)==8);assert(buffer.route[79]==5);assert(buffer.before==77&&buffer.after==88);
 wall=1;int n=full(&p);assert(n>0&&n<=200);verify(&p,n);
 /* Replanning native eight-step chunks must reach the original destination. */
 int total=0;while(p.from!=p.to){int steps=path_make(&p);assert(steps>0&&steps<=8);for(int i=0;i<steps;i++)assert(location_in_dir(p.from,p.rotations[i],&p.from));total+=steps;assert(total<=200);}
 p=query(1,180,100,200,buffer.route);gap=-1;assert(!full(&p));assert(!strcmp(path_trace.reason,"unreachable"));
 gap=150;n=full(&p);assert(n>0);verify(&p,n);gap=80;p.max_rotations=60;assert(!full(&p));p.max_rotations=200;wall=0;
 trap_mode=1;n=full(&p);assert(n>0);verify(&p,n);int64_t at=p.from;for(int i=0;i<n;i++){location_in_dir(at,p.rotations[i],&at);assert(at!=location_make(140,100));}
 trap_mode=2;n=full(&p);assert(n>0);verify(&p,n);trap_mode=0;
 door=1;p.flags=PATH_FLAG_0x0002;assert(!full(&p));p.flags=0;assert(full(&p)>0);door=0;
 window_mode=1;p.flags=PATH_FLAG_0x0004;assert(!full(&p));p.flags=0;assert(full(&p)>0);window_mode=0;
 p=query(1,180,100,200,buffer.route);occupied=1;assert(!path_make(&p));occupied=0;
 wall=1;p=query(1,130,100,200,buffer.route);gap=-1;assert(!path_make(&p));p.flags=PATH_FLAG_0x0001;n=full(&p);assert(n>0);at=p.from;for(int i=0;i<n;i++)location_in_dir(at,p.rotations[i],&at);assert(at!=p.to);wall=0;
 p=query(3,180,100,200,buffer.route);p.flags=PATH_FLAG_0x0200;dword_5DE5F8=clock_ms;dword_5DE5FC=100;assert(!full(&p));assert(!strcmp(path_trace.reason,"npc_quota"));p.obj=2;assert(full(&p)>0);
 p=query(1,180,100,200,buffer.route);timer_step=2;path_trace=(PathTrace){0};assert(!modern_path_search(&p,1));assert(!strcmp(path_trace.reason,"time_budget"));timer_step=0;
 p=query(1,301,100,200,buffer.route);assert(!full(&p));assert(!strcmp(path_trace.reason,"range"));
 p=query(1,150,100,16,buffer.route);assert(!path_make(&p));assert(buffer.before==77&&buffer.after==88);
 /* Near-map-boundary searches must never wrap negative tile coordinates. */
 p=query(1,40,2,200,buffer.route);p.from=location_make(2,2);n=full(&p);assert(n>0);verify(&p,n);
 FILE* f=fopen("ArcanumModern/pathfinding.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);clock_ms+=2000;
 p=query(1,301,100,200,buffer.route);errno=EDOM;assert(!path_make(&p));assert(errno==EDOM);
 for(int i=0;i<100;i++)assert(!path_make(&p));
 puts("PASS: 80-tile straight and detoured paths, eight-step replanning, walls/doors/windows/traps, adjacency, capacity, NPC quota, time budget, boundaries and log rotation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-pathfinding-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/pathfinding.previous.log').stat().st_size==1024*1024
 log=(p/'ArcanumModern/pathfinding.log').read_text()
 assert 'reason=range' in log and len(log.splitlines())==20
