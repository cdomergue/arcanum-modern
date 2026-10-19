#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_map_bitmap.inc').read_text()
code=r'''
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdlib.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {int x,y,w,h;} SDL_Rect;
typedef struct {int r,g,b,a;} SDL_Color;
typedef struct {int id;} SDL_Texture;
typedef struct {float x,y;} Point;
typedef struct {float r,g,b,a;} Color;
typedef struct {Point position;Color color;Point tex_coord;} SDL_Vertex;
typedef int SDL_BlendMode;
enum {SDL_BLENDMODE_BLEND};
typedef int TigVideoBuffer;
typedef struct {int flags,lerp_colors[4];} TigVideoBufferBlitInfo;
typedef struct {int townmap,width,height,offset_x,offset_y;} ModernMap;
static struct {int columns,rows,width,height;} map_local={3,1,300,100};
static void* renderer;
static int known[3]={1,1,0},loads[3],renders,geometry,destroyed;
static bool waitable;
static bool SDL_RenderClipEnabled(void* r){return false;}
static void SDL_GetRenderClipRect(void* r,SDL_Rect* rect){*rect=(SDL_Rect){0};}
static void SDL_SetRenderClipRect(void* r,SDL_Rect* rect){}
static void SDL_GetRenderDrawBlendMode(void* r,SDL_BlendMode* b){*b=0;}
static void SDL_SetRenderDrawBlendMode(void* r,SDL_BlendMode b){}
static void fill(SDL_FRect r,SDL_Color c){assert(c.r==0 && c.g==0 && c.b==0 && c.a==255);}
static bool townmap_is_waitable(int id){return waitable;}
static bool townmap_tile_blit_info(int id,int index,TigVideoBufferBlitInfo* fog){
    assert(index>=0 && index<3);
    if(index==1)*fog=(TigVideoBufferBlitInfo){.flags=1,.lerp_colors={255,128,0,64}};
    return known[index];
}
static TigVideoBuffer* modern_map_local_tile(int index){
    assert(waitable || known[index]);loads[index]++;static int buffer;buffer=index;return &buffer;
}
static void modern_map_local_tile_release(int index){}
static SDL_Texture* texture_from_buffer(TigVideoBuffer* b,float* w,float* h){
    SDL_Texture* t=malloc(sizeof(*t));t->id=*b;return t;
}
static void SDL_DestroyTexture(SDL_Texture* texture){if(texture){destroyed++;free(texture);}}
static SDL_FRect map_pixel_rect(SDL_FRect r){return r;}
static void SDL_RenderTexture(void* r,SDL_Texture* t,void* src,SDL_FRect* dst){renders++;}
static int tig_color_get_red(int color){return color;}
static void SDL_RenderGeometry(void* r,void* t,SDL_Vertex* vertices,int n,const int* indexes,int count){
    assert(n==4 && count==6);assert(vertices[0].color.a==0);assert(vertices[2].color.a==1);geometry++;
}
''' + source + r'''
int main(void){
    ModernMap map={.townmap=1,.width=300,.height=100};SDL_FRect dest={0,0,600,200};
    assert(map_bitmap_draw(&map,dest,2));assert(renders==2 && geometry==1 && !loads[2]);
    map_bitmap_draw(&map,dest,2);assert(loads[0]==1 && loads[1]==1 && !loads[2]);
    known[2]=1;map_bitmap_draw(&map,dest,2);assert(loads[2]==1);
    map_bitmap_clear();assert(destroyed==3 && !map_bitmap_count && !map_bitmap_tiles);
    known[2]=0;waitable=true;geometry=0;map_bitmap_draw(&map,dest,2);
    assert(loads[2]==2 && !geometry);map_bitmap_clear();
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-fog-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror','-Wno-unused-parameter',str(path/'test.c'),'-lm','-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: native map fallback hides unknown tiles, preserves partial fog, caches textures and releases them')
