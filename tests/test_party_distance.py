#!/usr/bin/env python3
"""Exercise production remote-control AI gates and normal follower catchup guards."""
from pathlib import Path
import subprocess, tempfile, os
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/game/ai.c').read_text()
def fn(name):
 start=s.index('\n'+name)+1
 return s[start:s.index('\n}',start)+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
typedef uint64_t Uint64;
enum {OBJ_HANDLE_NULL=0,OBJ_F_NPC_FLAGS,OBJ_F_CRITTER_FLAGS,OBJ_F_LOCATION,ONF_AI_WAIT_HERE=1,OCF_PARALYZED=2};
static bool dead,combat,active=true,leader=true;
static int64_t selected=2;
static int distance=45,flags,teleports,interrupts;
static int64_t centered;
static void location_origin_set(int64_t location){centered=location;}
static bool iso_redraw(void){return true;}
static void location_calc_dist_from_screen_center(int64_t loc,int64_t* x,int64_t* y){*x=loc-centered;*y=0;}
static Uint64 SDL_GetTicks(void){return 10000;}
static bool critter_is_dead(int64_t obj){(void)obj;return dead;}
static bool combat_turn_based_is_active(void){return combat;}
static int64_t modern_party_selected(void){return selected;}
static int64_t player_get_local_pc_obj(void){return 1;}
static int object_dist(int64_t a,int64_t b){(void)a;(void)b;return distance;}
static int64_t critter_pc_leader_get(int64_t obj){(void)obj;return leader?1:0;}
static int obj_field_int32_get(int64_t obj,int field){(void)obj;(void)field;return flags;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)obj;(void)field;return 100;}
static bool critter_is_active(int64_t obj){(void)obj;return active;}
static bool sub_424070(int64_t obj,int a,bool b,bool c){(void)obj;(void)a;(void)b;(void)c;interrupts++;return true;}
static void sub_43E770(int64_t obj,int64_t loc,int a,int b){(void)obj;(void)loc;(void)a;(void)b;teleports++;}
static int ai_distance_to_nearest_player(int64_t obj){(void)obj;return distance;}
''' +s[s.index('/* Bounded diagnostics'):s.index('// 0x4AD420',s.index('/* Bounded diagnostics'))]+fn('bool sub_4AD420(')+fn('bool ai_follower_catchup(')+fn('bool ai_follower_release(')+fn('int ai_timeevent_delay(')+r'''
int main(void){
 assert(sub_4AD420(2));assert(!ai_follower_catchup(2));assert(ai_timeevent_delay(2)==250);
 assert(!teleports&&!interrupts);
 selected=0;assert(!sub_4AD420(2));assert(ai_follower_catchup(2));assert(teleports==1&&interrupts==1);
 distance=30;assert(sub_4AD420(2));assert(!ai_follower_catchup(2));distance=45;
 bool* guards[]={&dead,&combat};
 for(int i=0;i<2;i++){*guards[i]=true;assert(!ai_follower_catchup(2));*guards[i]=false;}
 active=false;assert(!ai_follower_catchup(2));active=true;
 leader=false;assert(!ai_follower_catchup(2));leader=true;
 flags=ONF_AI_WAIT_HERE;assert(!ai_follower_catchup(2));flags=OCF_PARALYZED;assert(!ai_follower_catchup(2));
 assert(teleports==1&&interrupts==1);
 assert(!ai_follower_release(2,0)&&!centered);
 flags=0;assert(ai_follower_release(2,0)&&centered==100);
 FILE* f=fopen("ArcanumModern/party-distance.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);
 errno=EDOM;party_distance_log("test",2,45);assert(errno==EDOM);
 puts("PASS: remote manual AI stays active, catchup suppressed until release, native restrictions preserved, bounded logging");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-party-distance-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/party-distance.previous.log').stat().st_size==1024*1024
 assert 'event=test actor=2 distance=45' in (p/'ArcanumModern/party-distance.log').read_text()
