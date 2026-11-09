"""Read-only coordinate conversions for town, travel and continent maps."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "source/src/ui/modern_map_coordinates.inc").read_text(encoding="utf-8")
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
enum {WMAP_UI_MODE_WORLD,WMAP_UI_MODE_CONTINENT,WMAP_UI_MODE_TOWN};
#define LOCATION_GET_X(l) ((l)&0xffffffff)
#define LOCATION_GET_Y(l) (((l)>>32)&0xffffffff)
#define LOCATION_MAKE(x,y) ((x)|((y)<<32))
#define OBJ_F_LOCATION 1
typedef struct {int width,height;} Rect;
typedef struct {int x,y;} Coords;
typedef struct {int map_width,map_height;Rect field_14;Coords field_3C;} WmapInfo;
typedef struct {int width,height;float scale;int64_t loc;} TownMapInfo;
static bool wmap_ui_created=true,reject_location;
static int wmap_ui_mode=WMAP_UI_MODE_TOWN;
static WmapInfo wmap_ui_mode_info[3]={
    {2000,1200,{501,365},{1001,400}}, {400,240,{400,240},{0,0}}, {800,800,{501,365},{400,400}}};
static TownMapInfo wmap_ui_tmi={3200,3200,.25f,LOCATION_MAKE(3200LL,3200LL)};
static void location_limits_get(int64_t* x,int64_t* y){*x=6400;*y=6400;}
static void location_xy(int64_t loc,int64_t* x,int64_t* y){*x=LOCATION_GET_X(loc);*y=LOCATION_GET_Y(loc);}
static bool location_at(int64_t x,int64_t y,int64_t* loc){
    if(reject_location || x<0 || x>=6400 || y<0 || y>=6400)return false;
    *loc=LOCATION_MAKE(x,y);return true;
}
static int64_t player_get_local_pc_obj(void){return 1;}
static int64_t obj_field_int64_get(int64_t pc,int field){assert(pc==1&&field==OBJ_F_LOCATION);return LOCATION_MAKE(3200LL,3200LL);}
''' + source + r'''
int main(void){
    WmapInfo original[3];memcpy(original,wmap_ui_mode_info,sizeof(original));
    int x,y,w,s;
    assert(modern_map_position_coords(&w,&s)&&w==50&&s==50);
    assert(modern_map_point_coords(400,400,&w,&s)&&w==50&&s==50);
    assert(modern_map_coords_point(50,50,&x,&y)&&x==408&&y==408);
    assert(modern_map_point_coords(x,y,&w,&s)&&w==50&&s==50);
    assert(!modern_map_point_coords(-1,400,&w,&s));
    assert(!modern_map_point_coords(800,400,&w,&s));
    assert(!modern_map_coords_point(1,1,&x,&y)); /* on the world, outside this town */
    assert(!modern_map_coords_point(0,0,&x,&y));
    assert(!modern_map_coords_point(INT_MAX,INT_MAX,&x,&y));
    reject_location=true;w=s=777;
    assert(!modern_map_point_coords(400,400,&w,&s)&&w==777&&s==777);
    reject_location=false;wmap_ui_tmi.scale=0;
    assert(!modern_map_point_coords(400,400,&w,&s)&&!modern_map_coords_point(50,50,&x,&y));
    wmap_ui_tmi.scale=.25f;
    wmap_ui_mode=WMAP_UI_MODE_WORLD;
    assert(modern_map_position_coords(&w,&s)&&w==1001&&s==400);
    assert(modern_map_point_coords(1234,789,&w,&s)&&w==1234&&s==789);
    assert(modern_map_coords_point(w,s,&x,&y)&&x==1234&&y==789);
    assert(!modern_map_coords_point(2000,0,&x,&y)&&!modern_map_point_coords(0,1200,&w,&s));
    wmap_ui_mode=WMAP_UI_MODE_CONTINENT;
    assert(modern_map_position_coords(&w,&s)&&w==1001&&s==400);
    assert(modern_map_point_coords(200,120,&w,&s)&&w==1000&&s==600);
    assert(modern_map_coords_point(1234,789,&x,&y)&&x==246&&y==157);
    assert(modern_map_point_coords(x,y,&w,&s)&&w==1230&&s==785);
    assert(!modern_map_point_coords(400,240,&w,&s));
    assert(!memcmp(original,wmap_ui_mode_info,sizeof(original)));
    wmap_ui_created=false;
    assert(!modern_map_position_coords(&w,&s)&&!modern_map_point_coords(0,0,&w,&s)&&!modern_map_coords_point(0,0,&x,&y));
    puts("PASS: town/travel/overview coordinates, bounds, failed terrain projection and read-only queries");
}
'''
with tempfile.TemporaryDirectory(prefix="arcanum-map-coordinates-") as folder:
    path = Path(folder)
    (path / "test.c").write_text(code, encoding="utf-8")
    subprocess.run([os.environ.get("CC", "cc"), "-std=c17", "-Wall", "-Wextra", "-Werror",
                    str(path / "test.c"), "-o", str(path / "test")], check=True)
    subprocess.run([str(path / "test")], check=True)
