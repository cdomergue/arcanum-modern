#!/usr/bin/env python3
"""Check blocked world-map destinations using the production approach search."""
from pathlib import Path
import os, subprocess, tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_map_route.inc').read_text()
source = source[:source.index('enum {TOWN_ROUTE_SPAN=')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <limits.h>
typedef struct { int x,y; } WmapCoords;
typedef struct { int64_t loc; int start,steps; } WmapRouteWaypoint;
typedef struct { int length; WmapRouteWaypoint* waypoints; } WmapRoute;
typedef struct { WmapCoords field_3C; int map_width,map_height; } WmapInfo;
typedef struct { int64_t from,to; int max_rotations; int8_t* rotations; } WmapPathInfo;
enum { WMAP_ROUTE_TYPE_WORLD, WMAP_UI_MODE_WORLD=0 };
static WmapRouteWaypoint previous;
static WmapRoute wmap_ui_routes[1]={{0,&previous}};
static WmapInfo wmap_ui_mode_info[1]={{{5,5},30,30}};
static int64_t qword_66D850=320;
static bool known=true,blocked=true,reachable=true;
static int calls, budget;
static int64_t origin;
static int64_t location(int x,int y){return ((int64_t)y<<32)|(uint32_t)x;}
static int64_t player_get_local_pc_obj(void){return 1;}
static void sub_561800(WmapCoords* c,int64_t* loc){*loc=location(c->x,c->y);}
static void sub_561490(int64_t loc,WmapCoords* c){c->x=(uint32_t)loc;c->y=(int)(loc>>32);}
static int64_t area_get_location(int area){assert(area==17);return location(10,10);}
static int area_get_nearest_known_area(int64_t loc,int64_t pc,int64_t range){
 assert(pc==1 && range==320);
 WmapCoords c;sub_561490(loc,&c);
 return known && abs(c.x-10)<=5 && abs(c.y-10)<=5?17:0;
}
static int64_t sector_id_from_loc(int64_t loc){return loc;}
static bool sector_is_blocked(int64_t loc){return blocked && loc==location(10,10);}
static int wmap_path_make(WmapPathInfo* p){
 calls++;budget=p->max_rotations;origin=p->from;
 assert(!sector_is_blocked(p->to) && p->rotations && budget>0 && budget<=5000);
 return !reachable?0:(p->to==location(11,9)?2:10);
}
''' + source + r'''
int main(void){
 WmapCoords target={10,10};modern_world_route_approach(&target);
 assert(target.x==11 && target.y==9 && calls==8 && budget==5000 && origin==location(5,5));
 assert(sector_is_blocked(location(10,10)));
 calls=0;target=(WmapCoords){9,10};modern_world_route_approach(&target);
 assert(target.x==9 && target.y==10 && !calls);
 known=false;target=(WmapCoords){10,10};modern_world_route_approach(&target);
 assert(target.x==10 && target.y==10 && !calls);known=true;
 blocked=false;modern_world_route_approach(&target);assert(!calls);blocked=true;
 reachable=false;modern_world_route_approach(&target);
 assert(target.x==10 && target.y==10 && calls==120);reachable=true;
 wmap_ui_routes[0].length=1;previous=(WmapRouteWaypoint){location(8,8),100,20};
 calls=0;modern_world_route_approach(&target);
 assert(target.x==11 && target.y==9 && origin==location(8,8) && budget==4880);
 previous.start=4980;calls=0;target=(WmapCoords){10,10};modern_world_route_approach(&target);
 assert(target.x==10 && target.y==10 && !calls);
 return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-world-route-') as directory:
    path=Path(directory)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: blocked marker approach, shortest reachable neighbour, unknown and ordinary targets, route budget and unreachable destination')
