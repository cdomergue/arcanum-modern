#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'source/src/ui/modern_combat.inc').read_text()
start = source.index('static struct {', source.index('static bool tactical_active'))
code = source[start:source.index('static bool tactical_region', start)]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <math.h>
typedef uint64_t Uint64;
typedef uint32_t Uint32;
typedef int tig_sound_handle_t;
enum {SDL_SCANCODE_UNKNOWN,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_E,SDL_SCANCODE_COUNT};
enum {SDL_EVENT_KEY_DOWN,SDL_EVENT_KEY_UP,SDL_EVENT_MOUSE_BUTTON_DOWN,SDL_EVENT_MOUSE_BUTTON_UP,SDL_EVENT_MOUSE_WHEEL,SDL_EVENT_MOUSE_MOTION};
enum {TIG_MOUSE_BUTTON_LEFT,TIG_MOUSE_BUTTON_RIGHT,TIG_MOUSE_BUTTON_MIDDLE};
enum {TIG_SOUND_HANDLE_INVALID=-1,SND_INTERFACE_BUTTON_LOW_RELEASE=3015,OBJ_F_LOCATION=1};
#define SDL_BUTTON_MASK(b) (1u<<((b)-1))
typedef struct {int type;struct {int scancode;}key;struct {int button;}button;}SDL_Event;
static bool enabled=true,active=true,menu,valid=true,keys[SDL_SCANCODE_COUNT];
static bool tactical_has_preview,click_captured;
static int64_t tactical_actor=1,turn=1,x,y;
static int pending_action,pressed_action,sounds,volume=90,stops,redraws;
static Uint64 now=1000;
static Uint32 mouse_buttons;
static bool tactical_active(void){return active;}
static bool mainmenu_ui_is_active(void){return menu;}
static int64_t combat_turn_based_whos_turn_get(void){return turn;}
static bool combat_player_controls(int64_t actor){return actor==1||actor==2;}
static Uint64 SDL_GetTicks(void){return now;}
static bool obj_handle_is_valid(int64_t actor){return actor&&valid;}
static int64_t obj_field_int64_get(int64_t actor,int field){assert(field==OBJ_F_LOCATION);return actor;}
static void location_origin_get(int64_t* a,int64_t* b){*a=x;*b=y;}
static void location_calc_dist_from_screen_center(int64_t loc,int64_t* dx,int64_t* dy){*dx=loc*400-x;*dy=loc*200-y;}
static void location_origin_scroll(int64_t dx,int64_t dy){x+=dx;y+=dy;}
static void location_origin_set(int64_t loc){x=loc*400;y=loc*200;}
static Uint32 SDL_GetMouseState(void* a,void* b){(void)a;(void)b;return mouse_buttons;}
static const bool* SDL_GetKeyboardState(void* a){(void)a;return keys;}
static void scroll_stop(void){stops++;}
static void tig_mouse_cancel_button(int b){assert(b>=0&&b<=2);}
static tig_sound_handle_t gsound_play_sfx(int id,int loops){assert(id==3015&&loops==1);sounds++;return 1;}
static int tig_sound_get_volume(int s){assert(s==1);return volume;}
static void tig_sound_set_volume(int s,int v){assert(s==1);volume=v;}
static void iso_invalidate_rect(void* rect){assert(!rect);redraws++;}
''' + code + r'''
int main(void){
    assert(!modern_combat_input_blocked());
    turn=2;assert(modern_combat_input_blocked());tactical_actor=2;
    keys[SDL_SCANCODE_E]=true;mouse_buttons=SDL_BUTTON_MASK(1);
    tactical_camera_focus(2,true);
    assert(x==0&&y==0&&sounds==1&&volume==30&&pending_action==-1&&pressed_action==-1);
    now+=125;tactical_camera_tick();
    assert(x==400&&y==200&&modern_combat_input_blocked());
    now+=125;tactical_camera_tick();
    assert(x==800&&y==400&&modern_combat_input_blocked());
    SDL_Event event={.type=SDL_EVENT_KEY_DOWN,.key={SDL_SCANCODE_ESCAPE}};
    assert(!tactical_camera_event(&event));
    event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.button=3;assert(tactical_camera_event(&event));
    now+=499;assert(modern_combat_input_blocked());
    now++;assert(!modern_combat_input_blocked());
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;assert(tactical_camera_event(&event));
    event.button.button=1;assert(tactical_camera_event(&event));assert(!tactical_camera_event(&event));
    event.type=SDL_EVENT_KEY_UP;event.key.scancode=SDL_SCANCODE_E;
    assert(tactical_camera_event(&event));assert(!tactical_camera_event(&event));
    keys[SDL_SCANCODE_E]=false;mouse_buttons=0;
    tactical_camera_focus(1,false);now+=50;tactical_camera_tick();
    assert(x>400&&x<800&&sounds==1);
    tactical_camera_focus(2,false);now+=250;tactical_camera_tick();assert(x==800&&y==400);
    assert(modern_combat_input_blocked());menu=true;assert(!modern_combat_input_blocked());menu=false;
    now+=500;assert(!modern_combat_input_blocked());
    tactical_camera_focus(1,false);valid=false;tactical_camera_tick();assert(!modern_combat_input_blocked());valid=true;
    tactical_camera_focus(1,false);active=false;assert(!modern_combat_input_blocked());
    assert(stops==5&&redraws>=4);
    puts("PASS: smooth centering, 500 ms guard, held mouse/key releases, interrupted pan, pause and invalid target");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-camera-test-') as temporary:
    folder = Path(temporary)
    (folder/'test.c').write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(folder/'test.c'), '-lm', '-o', str(folder/'test')], check=True)
    subprocess.run([str(folder/'test')], check=True)
