#!/usr/bin/env python3
"""Exercise automatic local-map waypoints and atomic failure using production C."""
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_map_route.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef uint64_t Uint64;
#define LOCATION_GET_X(loc) ((uint32_t)(loc))
#define LOCATION_GET_Y(loc) ((uint32_t)((uint64_t)(loc)>>32))
static int64_t location_make(int64_t x,int64_t y){return (int64_t)(((uint64_t)y<<32)|(uint32_t)x);}
typedef struct {int x,y;} WmapCoords;
typedef struct {int field_0,field_4;WmapCoords coords;int64_t loc;int start,steps;} WmapRouteWaypoint;
typedef struct {WmapRouteWaypoint waypoints[30];int length,field_3C4;} WmapRoute;
typedef struct {int num;char* str;} MesFileEntry;
typedef struct {int type;char* str;} UiMessage;
enum {WMAP_ROUTE_TYPE_TOWN,OBJ_F_LOCATION,UI_MSG_TYPE_FEEDBACK};
static WmapRoute wmap_ui_routes[1];
static int wmap_ui_tmi,wmap_ui_worldmap_mes_file,wmap_ui_mode,dword_5C9AD8;
static int calls,blocked,feedback,refreshes;
static Uint64 ticks,probe_time;
static int64_t player_loc;
static Uint64 SDL_GetTicks(void){return ticks;}
static int64_t player_get_local_pc_obj(void){return 1;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)obj;(void)field;return player_loc;}
static void townmap_coords_to_loc(void* tmi,int x,int y,int64_t* loc){(void)tmi;*loc=location_make(x,y);}
static void townmap_loc_to_coords(void* tmi,int64_t loc,int* x,int* y){(void)tmi;*x=LOCATION_GET_X(loc);*y=LOCATION_GET_Y(loc);}
static void mes_get_msg(int f,MesFileEntry* e){(void)f;feedback=e->num;e->str="feedback";}
static void intgame_message_window_display_msg(UiMessage* m){(void)m;}
static void sub_5648E0(int i,int kind,bool flag){(void)i;(void)kind;(void)flag;}
static void sub_5649C0(void){}
static void refresh(void){refreshes++;}
static struct {void (*refresh)(void);} wmap_ui_mode_info[]={{refresh}};
static bool anim_preview_route_segment(int64_t obj,int64_t from,int64_t to){
    (void)obj;calls++;ticks+=probe_time;
    int dx=(int)LOCATION_GET_X(to)-(int)LOCATION_GET_X(from);
    int dy=(int)LOCATION_GET_Y(to)-(int)LOCATION_GET_Y(from);
    assert(dx>=-32 && dx<=32 && dy>=-32 && dy<=32);
    if(blocked==1)return false;
    if(blocked==2 && LOCATION_GET_X(to)==42 && LOCATION_GET_Y(to)==10)return false;
    return true;
}
''' + source + r'''
static void reset(void){
    memset(wmap_ui_routes,0,sizeof(wmap_ui_routes));
    calls=blocked=feedback=refreshes=0;ticks=probe_time=0;player_loc=location_make(10,10);
}
static void check_route(int x,int y,int expected){
    WmapCoords target={x,y};assert(modern_town_route_add(&target));
    WmapRoute* r=&wmap_ui_routes[0];assert(r->length==expected);
    int64_t previous=player_loc;
    for(int i=0;i<r->length;i++){
        assert(town_route_distance(previous,r->waypoints[i].loc)<=32);
        assert(previous!=r->waypoints[i].loc);previous=r->waypoints[i].loc;
    }
    assert(previous==location_make(x,y));assert(dword_5C9AD8==expected-1);
}
int main(void){
    reset();check_route(30,10,1);
    reset();check_route(100,10,3);check_route(150,10,5);
    WmapRoute before=wmap_ui_routes[0];WmapCoords far={202,10};
    assert(!modern_town_route_add(&far));assert(!memcmp(&before,&wmap_ui_routes[0],sizeof(before)));
    assert(feedback==611);
    reset();check_route(202,10,6);
    reset();check_route(100,100,3);
    reset();player_loc=location_make(100,100);check_route(10,10,3);
    reset();blocked=2;check_route(85,10,3);
    reset();check_route(20,10,1);blocked=1;before=wmap_ui_routes[0];
    WmapCoords target={100,10};assert(!modern_town_route_add(&target));
    assert(!memcmp(&before,&wmap_ui_routes[0],sizeof(before)));assert(feedback==610);
    assert(calls<=TOWN_ROUTE_PROBES+1);
    reset();probe_time=250;target=(WmapCoords){100,10};assert(!modern_town_route_add(&target));
    assert(calls==1 && !wmap_ui_routes[0].length);
    reset();target=(WmapCoords){10,10};assert(!modern_town_route_add(&target));assert(!calls);
    reset();target=(WmapCoords){1000,1000};assert(!modern_town_route_add(&target));assert(!calls);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-route-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: distant waypoints, diagonals, detours, appending, limits, timeout and atomic failure')
