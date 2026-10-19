#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/wmap_ui.c').read_text()
source=source[source.index('void modern_map_local_mouse('):source.index('void* modern_map_local_tile(')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
enum {WMAP_UI_MODE_TOWN,WMAP_UI_STATE_NORMAL,WMAP_UI_STATE_MOVING,WMAP_ROUTE_TYPE_TOWN,
    TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP,TIG_MESSAGE_MOUSE_MOVE};
typedef struct {int x,y;} WmapCoords;
typedef struct {WmapCoords coords;int field_4;} Waypoint;
typedef struct {int length;Waypoint waypoints[6];} WmapRoute;
typedef struct {int map_width,map_height;void (*refresh)(void);} WmapInfo;
static bool wmap_ui_created=true;
static int wmap_ui_mode=WMAP_UI_MODE_TOWN,wmap_ui_state=WMAP_UI_STATE_NORMAL;
static WmapRoute wmap_ui_routes[4];
static int dword_5C9AD8,dword_65E968,added,deleted=-1,selected=-1,refreshes;
static WmapCoords destination;
static void refresh(void){refreshes++;}
static WmapInfo wmap_ui_mode_info[2]={{1000,800,refresh},{1000,800,refresh}};
static void sub_5649D0(int kind){selected=kind;}
static void sub_5643E0(WmapCoords* c){added++;destination=*c;}
static void sub_564840(int i){deleted=i;}
static void sub_5649C0(void){}
static void wmap_ui_state_set(int state){wmap_ui_state=state;}
''' + source + r'''
int main(void){
    modern_map_local_mouse(400,300,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);
    assert(added==1 && destination.x==400 && destination.y==300);
    modern_map_local_mouse(-1,300,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);
    modern_map_local_mouse(1000,300,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);
    modern_map_local_mouse(400,800,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);
    modern_map_local_mouse(400,300,TIG_MESSAGE_MOUSE_MOVE,10);assert(added==1);
    wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN]=(WmapRoute){.length=2,.waypoints={{{400,300},7},{{500,350},8}}};
    modern_map_local_mouse(405,305,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);assert(selected==7 && added==1);
    modern_map_local_mouse(530,350,TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP,10);assert(deleted==-1);
    modern_map_local_mouse(530,350,TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP,40);assert(deleted==1);
    wmap_ui_state=WMAP_UI_STATE_MOVING;
    modern_map_local_mouse(700,600,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);assert(added==1);
    modern_map_route_clear();assert(wmap_ui_state==WMAP_UI_STATE_NORMAL);
    assert(!wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length && dword_5C9AD8==-1 && refreshes==1);
    wmap_ui_mode=1;modern_map_local_mouse(400,300,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP,10);assert(added==1);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-input-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: local map absolute clicks, bounds, zoom-scaled waypoint hit targets, route cancellation and mode isolation')
