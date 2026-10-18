#!/usr/bin/env python3
"""Exercise production map gestures, bounded panning and live HUD icon selection."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1] / 'source/src/ui'
panels = (root / 'modern_panels.inc').read_text()
engine = (root / 'wmap_ui.c').read_text()
hud = (root / 'intgame.c').read_text()
gesture = panels[panels.index('static bool map_drag_event('):panels.index('static bool panels_event(')]
pan = engine[engine.index('void modern_map_pan('):]
icons = hud[hud.index('int modern_intgame_icon('):hud.index('void modern_inventory_details(')]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {SDL_FRect rect;int x,y,w,h,kind;} PanelRegion;
enum {SDL_EVENT_MOUSE_MOTION,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP};
enum {SDL_BUTTON_LEFT=1,SDL_BUTTON_RIGHT=3};
enum {TIG_MESSAGE_MOUSE_MOVE,TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP};
static bool map_pressed,map_dragged,captured;
static PanelRegion map_drag_region;
static float mouse_x,mouse_y,map_start_x,map_start_y,map_last_x,map_last_y,map_pan_x,map_pan_y;
static int map_press_x,map_press_y,events,types[3],click_x,click_y;
static bool inside(SDL_FRect r,float x,float y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
static void SDL_CaptureMouse(bool on){captured=on;}
static void panel_mouse_queue(int kind,int x,int y,int event){assert(kind==3&&events<3);types[events++]=event;click_x=x;click_y=y;}
typedef struct {int x,y,width,height;} TigRect;
typedef struct {int field_34,field_38,field_60,field_64;TigRect rect;void(*refresh_rect)(TigRect*);} WmapInfo;
static WmapInfo wmap_ui_mode_info[3];
static bool wmap_ui_created=true;
static int wmap_ui_state,wmap_ui_mode,refreshes;
#define WMAP_UI_STATE_MOVING 2
static void refresh(TigRect* r){(void)r;refreshes++;}
typedef enum {UI_PRIMARY_BUTTON_CHAR,UI_PRIMARY_BUTTON_LOGBOOK,UI_PRIMARY_BUTTON_TOWNMAP,UI_PRIMARY_BUTTON_WORLDMAP,UI_PRIMARY_BUTTON_INVENTORY} UiPrimaryButton;
static UiPrimaryButton intgame_map_button=UI_PRIMARY_BUTTON_TOWNMAP;
static int intgame_ui_primary_button_icons[]={169,187,193,194,186};
''' + gesture + pan + icons + r'''
int main(void){
 PanelRegion r={{100,100,800,400},20,40,400,200,3};
 mouse_x=300;mouse_y=200;
 assert(map_drag_event(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,&r));
 assert(captured&&events==0);
 /* Holding without moving does not generate a route before release. */
 for(int i=0;i<100;i++)map_drag_event(SDL_EVENT_MOUSE_MOTION,0,NULL);
 assert(events==0&&!map_dragged);
 mouse_x+=2;map_drag_event(SDL_EVENT_MOUSE_MOTION,0,NULL);
 map_drag_event(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,NULL);
 assert(events==3&&!captured&&!map_pressed&&click_x==120&&click_y==90);
 assert(types[0]==TIG_MESSAGE_MOUSE_MOVE&&types[1]==TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN&&types[2]==TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP);
 events=0;mouse_x=300;mouse_y=200;
 map_drag_event(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_LEFT,&r);
 mouse_x=310;mouse_y=220;map_drag_event(SDL_EVENT_MOUSE_MOTION,0,NULL);
 assert(map_dragged&&map_pan_x==-5&&map_pan_y==-10&&events==0);
 /* Capture survives leaving the viewport; releasing there never places a route. */
 mouse_x=1100;map_drag_event(SDL_EVENT_MOUSE_MOTION,0,NULL);
 map_drag_event(SDL_EVENT_MOUSE_BUTTON_UP,SDL_BUTTON_LEFT,NULL);
 assert(!captured&&!map_pressed&&events==0);
 assert(!map_drag_event(SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_BUTTON_RIGHT,&r));
 for(wmap_ui_mode=0;wmap_ui_mode<3;wmap_ui_mode++){
  WmapInfo* info=&wmap_ui_mode_info[wmap_ui_mode];
  *info=(WmapInfo){.field_34=50,.field_38=50,.field_60=100,.field_64=80,.refresh_rect=refresh};
  modern_map_pan(-200,200);assert(info->field_34==0&&info->field_38==80);
  int before=refreshes;modern_map_pan(-1,1);assert(refreshes==before);
  wmap_ui_state=WMAP_UI_STATE_MOVING;modern_map_pan(10,-10);assert(info->field_34==0&&info->field_38==80);wmap_ui_state=0;
 }
 assert(modern_intgame_icon(0)==169&&modern_intgame_icon(2)==187);
 intgame_ui_primary_button_icons[UI_PRIMARY_BUTTON_CHAR]=558;
 intgame_ui_primary_button_icons[UI_PRIMARY_BUTTON_LOGBOOK]=560;
 assert(modern_intgame_icon(0)==558&&modern_intgame_icon(2)==560);
 intgame_map_button=UI_PRIMARY_BUTTON_WORLDMAP;assert(modern_intgame_icon(3)==194);
 intgame_ui_primary_button_icons[UI_PRIMARY_BUTTON_WORLDMAP]=195;assert(modern_intgame_icon(3)==195);
 intgame_ui_primary_button_icons[UI_PRIMARY_BUTTON_LOGBOOK]=187;assert(modern_intgame_icon(2)==187);
 puts("PASS: held clicks, drag threshold, capture outside map, viewport bounds, travel and notification icons");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-qol-test-') as directory:
    source = Path(directory) / 'qol.c'
    binary = Path(directory) / 'qol'
    source.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
