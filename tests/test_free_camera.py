#!/usr/bin/env python3
"""Exercise immediate edge advances and camera ownership."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <math.h>
typedef uint64_t Uint64;
enum {OBJ_F_LOCATION,OBJ_F_OFFSET_X,OBJ_F_OFFSET_Y};
static bool follow_camera,allowed=true,menu_suspended,slide,tactical;
static int64_t selected=1,dx[3],dy[3],origin_x,origin_y;
static int width=1600,height=900,pans,invalidations;
static Uint64 now=1000;
static Uint64 SDL_GetTicks(void){return now;}
static int64_t follow_control_actor(void){return selected;}
static bool obj_handle_is_valid(int64_t actor){return actor>0&&actor<3;}
static bool world_zoom_allowed(void){return allowed;}
static bool slide_ui_is_active(void){return slide;}
static bool tactical_active(void){return tactical;}
static int hrp_iso_window_width_get(void){return width;}
static int hrp_iso_window_height_get(void){return height;}
static int64_t obj_field_int64_get(int64_t actor,int field){(void)field;return actor;}
static int obj_field_int32_get(int64_t actor,int field){(void)actor;(void)field;return 0;}
static void location_calc_dist_from_screen_center(int64_t actor,int64_t* x,int64_t* y){*x=dx[actor];*y=dy[actor];}
static void location_origin_scroll(int64_t x,int64_t y){origin_x+=x;origin_y+=y;for(int i=1;i<3;i++){dx[i]-=x;dy[i]-=y;}pans++;}
static void scroll_by(int64_t x,int64_t y){location_origin_scroll(x,y);}
static void iso_invalidate_rect(void* rect){assert(!rect);invalidations++;}
''' +(root/'source/src/ui/modern_free_camera.inc').read_text()+r'''

int main(void){
 dx[1]=100;dy[1]=100;modern_free_camera_keep_visible(1);assert(!pans);
 dx[1]=700;dy[1]=-400;modern_free_camera_keep_visible(1);
 assert(pans==1&&origin_x==1020&&origin_y==-535);assert(dx[1]==-320&&dy[1]==135);
 for(int i=0;i<10;i++){now+=100;modern_free_camera_keep_visible(1);}assert(pans==1);
 selected=2;dx[2]=-800;dy[2]=400;
 modern_free_camera_keep_visible(1);assert(pans==1);
 modern_free_camera_keep_visible(2);assert(pans==2&&dx[2]==320&&dy[2]==-135);
 width=3840;height=2160;dx[2]=1700;dy[2]=-900;
 modern_free_camera_keep_visible(2);assert(dx[2]==-768&&dy[2]==324);
 width=800;height=600;dx[2]=-350;dy[2]=250;
 modern_free_camera_keep_visible(2);assert(dx[2]==160&&dy[2]==-90);
 width=1600;height=900;selected=1;dx[1]=700;dy[1]=200;
 int before=pans;modern_free_camera_keep_visible(1);assert(pans==before+1&&dx[1]==-320&&dy[1]==-135);
 /* Suppressed contexts must never schedule or apply a delayed translation. */
 for(int context=0;context<5;context++){
  dx[1]=700;
  if(context==0)follow_camera=true;
  if(context==1)allowed=false;
  if(context==2)menu_suspended=true;
  if(context==3)slide=true;
  if(context==4)tactical=true;
  before=pans;modern_free_camera_keep_visible(1);assert(pans==before);
  follow_camera=false;allowed=true;menu_suspended=slide=tactical=false;
 }
 FILE* f=fopen("ArcanumModern/free-camera.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);now+=2000;
 for(int i=0;i<100;i++){errno=EDOM;free_camera_log("jump",1,700,400,800,600,860,490,1,0);assert(errno==EDOM);}
 puts("PASS: immediate broad advances, no repeated edge nudges, diagonal lookahead, actor selection, zoom, context isolation and bounded diagnostics");
}

'''
animation=(root/'source/src/game/anim.c').read_text()
assert 'if (new_loc != loc) {\n                modern_free_camera_keep_visible(obj);' in animation
ui=(root/'source/src/ui/modern_ui.c').read_text()
assert 'modern_free_camera_tick' not in ui
with tempfile.TemporaryDirectory(prefix='arcanum-free-camera-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/free-camera.previous.log').stat().st_size==1024*1024
 log=(p/'ArcanumModern/free-camera.log').read_text();assert len(log.splitlines())==20 and 'frames=1 duration_ms=0' in log
