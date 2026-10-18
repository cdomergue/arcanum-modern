#!/usr/bin/env python3
"""Check real wheel routing, bounds and resize transitions with a simulated display."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_ui.c').read_text()
start = source.index('static bool hud_region(float x,float y);')
end = source.index('void modern_ui_menu_tick(void)', start)
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#define SDL_EVENT_MOUSE_WHEEL 1
#define SDL_MOUSEWHEEL_FLIPPED 1
#define SDL_BUTTON_LMASK 1
#define SDL_BUTTON_RMASK 2
#define SDL_BUTTON_MMASK 4
#define INTGAME_MODE_MAIN 0
#define SDL_Log(...) ((void)0)
typedef struct {int type;struct {float mouse_x,mouse_y,y;int direction;} wheel;} SDL_Event;
static bool enabled=true,in_game=true,journal,menu,panel,compact,fail_resize;
static int mode,buttons,logical_w=1280,logical_h=720,window_w=1920,window_h=1080,resizes;
static float canvas_w=1600,canvas_h=900,origin_x,origin_y;
static void *window,*renderer;
static bool visible(void){return enabled && in_game;}
static bool mainmenu_ui_is_active(void){return menu;}
static int native_panel(void){return panel;}
static bool intgame_is_compact_interface(void){return compact;}
static int intgame_mode_get(void){return mode;}
static int hrp_iso_window_width_get(void){return logical_w;}
static int hrp_iso_window_height_get(void){return logical_h;}
static void SDL_GetWindowSize(void* win,int* w,int* h){(void)win;*w=window_w;*h=window_h;}
static unsigned SDL_GetMouseState(float* x,float* y){(void)x;(void)y;return buttons;}
static bool SDL_RenderCoordinatesFromWindow(void* r,float x,float y,float* rx,float* ry){
    (void)r;float scale=fminf((float)window_w/logical_w,(float)window_h/logical_h);
    *rx=(x-(window_w-logical_w*scale)/2)/scale;
    *ry=(y-(window_h-logical_h*scale)/2)/scale;return true;
}
static bool hud_region(float x,float y){(void)x;return y<70 || y>770;}
static bool modern_world_resize(int w,int h,float ax,float ay){
    if(fail_resize)return false;
    assert(w>=800 && h>=600 && w<=4096 && h<=4096);
    origin_x+=(int)((w-logical_w)*ax);origin_y+=(int)((h-logical_h)*ay);
    logical_w=w;logical_h=h;resizes++;return true;
}
''' + source[start:end] + r'''
int main(void){
    world_zoom_sync();assert(resizes==0);
    SDL_Event e={.type=1,.wheel={1200,650,-1,0}};
    assert(world_zoom_event(&e));assert(zoom_pending && world_zoom<1);
    assert(fabsf(zoom_anchor_x-1200.0f/1920)<.001);
    int before_w=logical_w,before_h=logical_h;
    world_zoom_sync();assert(resizes==1 && logical_w>1280 && logical_h>720);
    /* Cursor anchor stays on the same world pixel, within integer rounding. */
    assert(fabsf((logical_w-before_w)*zoom_anchor_x-origin_x)<=1);
    assert(fabsf((logical_h-before_h)*zoom_anchor_y-origin_y)<=1);
    for(int i=0;i<40;i++)world_zoom_event(&e);
    world_zoom_sync();assert(logical_w==2560 && logical_h==1440 && world_zoom==.5f);
    int calls=resizes;world_zoom_sync();assert(resizes==calls);
    menu=true;assert(!world_zoom_event(&e));world_zoom_sync();assert(logical_w==1280);
    menu=false;world_zoom_sync();assert(logical_w==2560);
    panel=true;assert(!world_zoom_event(&e));world_zoom_sync();assert(logical_w==1280);
    panel=false;world_zoom_sync();assert(logical_w==2560);
    enabled=false;world_zoom_sync();assert(logical_w==1280 && !world_zoom_event(&e));
    enabled=true;world_zoom_sync();assert(logical_w==2560);
    e.wheel.y=1;
    for(int i=0;i<40;i++)world_zoom_event(&e);
    world_zoom_sync();assert(logical_h==600 && fabsf(world_zoom-1.2f)<.001);
    float saved=world_zoom;e.wheel.mouse_y=1040;world_zoom_event(&e);assert(world_zoom==saved);
    e.wheel.mouse_y=650;buttons=1;world_zoom_event(&e);assert(world_zoom==saved);buttons=0;
    e.wheel.direction=1;world_zoom_event(&e);assert(world_zoom<saved);world_zoom_sync();
    saved=world_zoom;e.wheel.direction=0;e.wheel.y=-.25f;world_zoom_event(&e);
    assert(world_zoom<saved && world_zoom>saved/1.12f);world_zoom_sync();
    /* Pillarboxing: derive anchors from the actual world viewport. */
    window_w=2560;window_h=1080;e.wheel.mouse_x=640;e.wheel.mouse_y=540;
    world_zoom_event(&e);assert(fabsf(zoom_anchor_x-1.0f/6)<.002);
    world_zoom_sync();saved=world_zoom;e.wheel.mouse_x=20;world_zoom_event(&e);assert(world_zoom==saved);
    /* Failed allocation keeps the current world, rather than a false zoom state. */
    e.wheel.mouse_x=1280;world_zoom_event(&e);fail_resize=true;world_zoom_sync();
    assert(fabsf(world_zoom-(float)1280/logical_w)<.001 && !zoom_pending);
    puts("PASS: wheel direction, fractional steps, limits, cursor anchor, letterboxing, modal/F9 restore and allocation failure");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-zoom-test-') as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra',str(c),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
