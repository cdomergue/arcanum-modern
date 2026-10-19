#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_map_travel.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef uint64_t Uint64;
#define OBJ_HANDLE_NULL 0
enum {WMAP_ROUTE_TYPE_TOWN,OBJ_F_LOCATION,UI_MSG_TYPE_FEEDBACK};
typedef struct {int64_t loc;} WmapRouteWaypoint;
typedef struct {WmapRouteWaypoint waypoints[6];int length;} WmapRoute;
typedef struct {int num;char* str;} MesFileEntry;
typedef struct {int type;char* str;} UiMessage;
static WmapRoute wmap_ui_routes[1];
static int wmap_ui_worldmap_mes_file;
static int feedback,closed,moves,queued,previews,failed_preview,failed_queue;
static bool can_start=true,idle,valid=true,combat,modern=true;
static int map_id=1;
static int64_t player=1,location;
static Uint64 ticks=100;
static Uint64 SDL_GetTicks(void){return ticks;}
static int64_t player_get_local_pc_obj(void){return player;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)obj;(void)field;assert(valid);return location;}
static void mes_get_msg(int f,MesFileEntry* e){(void)f;assert(e->num==610);e->str="feedback";}
static void intgame_message_window_display_msg(UiMessage* m){(void)m;feedback++;}
static bool anim_preview_route_segment(int64_t obj,int64_t from,int64_t to){
    (void)obj;(void)from;(void)to;return ++previews!=failed_preview;
}
static bool anim_goal_move_to_tile(int64_t obj,int64_t loc){(void)obj;(void)loc;moves++;return can_start;}
static bool sub_433A00(int64_t obj,int64_t loc,bool client){(void)obj;(void)loc;(void)client;return ++queued!=failed_queue;}
static bool tig_net_is_active(void){return false;}
static bool tig_net_is_host(void){return true;}
static int map_current_map(void){return map_id;}
static void sub_436D20(int flags,int zero){(void)flags;(void)zero;}
static void wmap_ui_close(void){closed++;}
static bool modern_ui_enabled(void){return modern;}
static bool obj_handle_is_valid(int64_t obj){(void)obj;return valid;}
static bool combat_critter_is_combat_mode_active(int64_t obj){(void)obj;return combat;}
static bool sub_423300(int64_t obj,void* id){(void)obj;(void)id;return true;}
static bool anim_is_idle(int64_t obj){(void)obj;return idle;}
''' + source + r'''
static void reset(void){
    modern_map_route_cancel_tracking();
    wmap_ui_routes[0]=(WmapRoute){.length=3,.waypoints={{10},{20},{30}}};
    feedback=closed=moves=queued=previews=failed_preview=failed_queue=0;
    can_start=valid=modern=true;idle=combat=false;map_id=1;player=1;location=0;ticks=100;
}
int main(void){
    reset();failed_preview=2;assert(modern_town_route_start());assert(feedback==1 && !moves && !closed);
    reset();can_start=false;modern_town_route_start();assert(feedback==1 && moves==1 && !closed && !queued);
    reset();failed_queue=1;modern_town_route_start();assert(feedback==1 && queued==1 && closed==1 && !town_travel_actor);
    reset();modern_town_route_start();assert(previews==3 && moves==1 && queued==2 && closed==1 && !feedback);
    modern_map_route_tick();assert(town_travel_actor);
    location=30;modern_map_route_tick();assert(!town_travel_actor && !feedback);
    reset();modern_town_route_start();idle=true;modern_map_route_tick();ticks+=749;
    modern_map_route_tick();assert(!feedback);ticks++;modern_map_route_tick();assert(feedback==1);
    modern_map_route_tick();assert(feedback==1);
    reset();modern_town_route_start();idle=true;modern_map_route_tick();idle=false;ticks+=1000;
    modern_map_route_tick();assert(!feedback && !town_travel_idle_since);
    reset();modern_town_route_start();valid=false;modern_map_route_tick();assert(!town_travel_actor && !feedback);
    reset();modern_town_route_start();map_id=2;modern_map_route_tick();assert(!town_travel_actor && !feedback);
    reset();modern_town_route_start();combat=true;modern_map_route_tick();assert(!town_travel_actor && !feedback);
    reset();modern_town_route_start();modern_map_route_cancel_tracking();idle=true;ticks+=10000;
    modern_map_route_tick();assert(!town_travel_actor && !feedback);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-travel-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: native route preflight, queue rejection, completion, blocked feedback and interruption tracking')
