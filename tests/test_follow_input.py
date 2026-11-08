#!/usr/bin/env python3
"""Exercise production follow movement routing, stopping and bounded diagnostics."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'source/src/ui/modern_ui.c').read_text()
def function(name):
    start = source.index('static int64_t '+name+'(void)')
    return source[start:source.index('\n}', start)+2]
walk = source[source.index('typedef struct FollowWalk'):source.index('\nvoid modern_ui_tick(void)')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
typedef uint64_t Uint64;
typedef struct { int slot; } AnimID;
typedef struct { int curr,max; int64_t field_E8; unsigned char rotations[64]; } AnimPath;
typedef struct { int64_t anim_obj; struct { int type; struct {int64_t loc;} params[1]; } goals[1]; AnimPath path; } AnimRunInfo;
enum {AG_MOVE_TO_TILE,AG_RUN_TO_TILE,AGDATA_TARGET_TILE=0,OBJ_F_LOCATION=0,INTGAME_MODE_MAIN=0,SDL_SCANCODE_W=0,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D};
static int64_t selected,positions[4]={0,100,200,300};
static bool active=true,allowed=true,keys[4],valid[4]={false,true,true,true},path_ok=true;
static unsigned follow_walk_taps;
static Uint64 now=1000;
static AnimRunInfo runs[4];
static int commands[4];
static int64_t player_get_local_pc_obj(void){return 1;}
static int64_t modern_party_selected(void){return valid[selected]?selected:0;}
static bool obj_handle_is_valid(int64_t obj){return valid[obj];}
static bool critter_is_active(int64_t obj){return valid[obj];}
static bool modern_follow_camera_active(void){return active;}
static bool modern_camera_enabled(void){return allowed;}
static int intgame_mode_get(void){return 0;}
static bool modern_camera_key(int key){return keys[key];}
static Uint64 SDL_GetTicks(void){return now;}
static bool anim_id_to_run_info(AnimID* id,AnimRunInfo** run){*run=&runs[id->slot];return (*run)->anim_obj!=0;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)field;return positions[obj];}
static bool location_in_dir(int64_t tile,int dir,int64_t* out){(void)dir;*out=tile+1;return true;}
static bool location_in_range(int64_t tile,int dir,int steps,int64_t* out){(void)dir;*out=tile+steps;return true;}
static int location_dist(int64_t a,int64_t b){return abs((int)(b-a));}
static bool tig_net_is_active(void){return false;}
static bool anim_preview_path(int64_t obj,int64_t dest,bool flag,AnimPath* path){(void)flag;*path=(AnimPath){.max=dest-positions[obj]};return path_ok;}
static bool anim_goal_move_to_tile(int64_t obj,int64_t dest){commands[obj]++;runs[obj]=(AnimRunInfo){.anim_obj=obj,.path={.curr=0,.max=8,.field_E8=positions[obj]}};runs[obj].goals[0].params[0].loc=dest;return true;}
static bool sub_423300(int64_t obj,AnimID* id){id->slot=obj;return true;}
''' + function('follow_control_actor') + '\n' + walk + r'''
int main(void){
 assert(follow_control_actor()==1);
 keys[0]=true;follow_walk_tick();assert(follow_walk.pc==1 && commands[1]==1);
 /* Hold W while changing selection: finish the old actor's step and route anew. */
 selected=2;now+=16;follow_walk_tick();
 assert(follow_control_actor()==2 && follow_walk.pc==2 && commands[1]==2 && commands[2]==1);
 int hero_commands=commands[1];
 now+=300;follow_walk_tick();assert(commands[1]==hero_commands);
 /* A delayed release must shorten the companion's path too. */
 keys[0]=false;now+=16;follow_walk_tick();assert(!follow_walk.pc && commands[2]==2);
 /* Short taps, a second companion and deselection all use the same routing. */
 selected=3;follow_walk_taps=1;now+=16;follow_walk_tick();assert(follow_walk.pc==3);
 keys[0]=true;selected=0;now+=16;follow_walk_tick();assert(follow_walk.pc==1 && commands[3]==1);
 selected=2;now+=16;follow_walk_tick();assert(follow_walk.pc==2);
 valid[2]=false;now+=16;follow_walk_tick();assert(follow_walk.pc==1);
 /* Opening a menu while held stops movement; release in the menu stays stopped. */
 allowed=false;now+=16;follow_walk_tick();assert(!follow_walk.pc);
 keys[0]=false;allowed=true;now+=16;follow_walk_tick();assert(!follow_walk.pc);
 /* A path failure never redirects movement back to the hero. */
 selected=3;path_ok=false;keys[0]=true;now+=200;hero_commands=commands[1];
 follow_walk_tick();assert(!follow_walk.pc && commands[1]==hero_commands);
 /* Rotation is bounded and preserves errno even when the directory is absent. */
 FILE* f=fopen("ArcanumModern/follow-input.log","wb");assert(f);
 assert(!fseek(f,1024*1024-1,SEEK_SET));fputc('x',f);fclose(f);
 errno=EDOM;follow_walk_log("test",3,0,1,0,1);assert(errno==EDOM);
 assert(rename("ArcanumModern","hidden")==0);
 errno=EDOM;follow_walk_log("unwritable",3,0,1,0,0);assert(errno==EDOM);
 assert(rename("hidden","ArcanumModern")==0);
 puts("PASS: held selection changes, companion release, taps, invalid selection, menus, blocked paths and log rotation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-follow-input-') as folder:
    path=Path(folder);(path/'ArcanumModern').mkdir()
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],cwd=path,check=True)
    assert (path/'ArcanumModern/follow-input.previous.log').stat().st_size == 1024*1024
    log=(path/'ArcanumModern/follow-input.log').read_text()
    assert 'event=test actor=3 hero=1' in log and 'ticks_ms=' in log and 'outcome=1' in log
