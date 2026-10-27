#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/location.h"
#include "game/sector.h"
#include "game/townmap.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include "../source/src/ui/wmap_ui.c"

int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    printf("Test process %ld, isolated directory %s\n",(long)getpid(),argv[1]);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};
    init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;
    init.texture_width=init.texture_height=2048;
    init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;
    init.sound_file_path_resolver=gsound_resolve_path;
    if(tig_init(&init)!=TIG_OK)return 3;
    intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};
    if(!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game))return 4;
    if(!gameuilib_init(&game) || !gamelib_mod_load("Arcanum") || !gameuilib_mod_load())return 5;
    gamelib_splash_clear();
    if(!modern_ui_init() || !gamelib_load(argv[2]))return 6;
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(!sub_45B300());

    int64_t pc=player_get_local_pc_obj();
    object_flags_unset(pc,OF_OFF);
    assert(critter_is_active(pc));
    int64_t from=obj_field_int64_get(pc,OBJ_F_LOCATION),destination=0,points[TOWN_ROUTE_PROBES];
    wmap_ui_open();
    assert(wmap_ui_is_created() && wmap_ui_mode==WMAP_UI_MODE_TOWN);
    wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN].length=0;
    const int directions[][2]={{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    WmapCoords coords={0};
    for(int distance=80;distance>=40 && !destination;distance-=8) {
        for(int direction=0;direction<8 && !destination;direction++) {
            int64_t candidate=location_make(LOCATION_GET_X(from)+directions[direction][0]*distance,
                LOCATION_GET_Y(from)+directions[direction][1]*distance);
            townmap_loc_to_coords(&wmap_ui_tmi,candidate,&coords.x,&coords.y);
            townmap_coords_to_loc(&wmap_ui_tmi,coords.x,coords.y,&candidate);
            TownRouteSearch search={.actor=pc,.deadline=SDL_GetTicks()+250};
            if(town_route_plan(&search,from,candidate,TOWN_ROUTE_PROBES,points)>1)destination=candidate;
        }
    }
    assert(destination && !anim_preview_route_segment(pc,from,destination));
    assert(sub_5643E0(&coords));
    WmapRoute* route=&wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN];
    assert(route->length>1 && route->length<=TOWN_ROUTE_PROBES);
    int64_t previous=from;
    for(int i=0;i<route->length;i++) {
        assert(anim_preview_route_segment(pc,previous,route->waypoints[i].loc));
        previous=route->waypoints[i].loc;
    }
    assert(previous==destination);
    /* Exercise growth past both former caps with a real reachable loop. */
    WmapCoords origin;townmap_loc_to_coords(&wmap_ui_tmi,from,&origin.x,&origin.y);
    int64_t roundtrip;townmap_coords_to_loc(&wmap_ui_tmi,origin.x,origin.y,&roundtrip);
    while(route->length<40) {
        assert(modern_town_route_add(&origin));
        assert(modern_town_route_add(&coords));
    }
    assert(route->length>30 && route->capacity>=route->length);
    ModernLocalMap state;assert(modern_map_local_state(&state) && state.count==route->length);
    for(int i=0;i<route->length;i++) {
        int x,y;assert(modern_map_local_point(i+1,&x,&y));
        assert(x==route->waypoints[i].coords.x && y==route->waypoints[i].coords.y);
    }
    /* Use short real segments to keep the movement test practical. */
    int64_t first=0,neighbor=0;
    for(int distance=1;distance<=8 && !neighbor;distance++)for(int i=0;i<8 && !neighbor;i++) {
        int64_t candidate=location_make(LOCATION_GET_X(from)+directions[i][0]*distance,LOCATION_GET_Y(from)+directions[i][1]*distance);
        if(!anim_preview_route_segment(pc,from,candidate))continue;
        for(int j=0;j<8 && !neighbor;j++) {
            int64_t next=location_make(LOCATION_GET_X(candidate)+directions[j][0],LOCATION_GET_Y(candidate)+directions[j][1]);
            if(next!=from && anim_preview_route_segment(pc,candidate,next) && anim_preview_route_segment(pc,next,candidate)) {
                first=candidate;neighbor=next;
            }
        }
    }
    assert(neighbor);route->length=40;
    for(int i=0;i<40;i++)route->waypoints[i].loc=i%2?neighbor:first;
    destination=neighbor;
    assert(critter_is_active(pc) && !combat_turn_based_is_active());
    printf("Following %d verified waypoints from (%d,%d) to (%d,%d)\n",route->length,
        (int)LOCATION_GET_X(from),(int)LOCATION_GET_Y(from),(int)LOCATION_GET_X(destination),(int)LOCATION_GET_Y(destination));
    fflush(stdout);
    modern_map_action(3);
    assert(!wmap_ui_is_created());
    Uint64 deadline=SDL_GetTicks()+60000;
    assert(town_travel_count==40);
    int reached=0,camera_checks=0;
    int64_t previous_loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
    while(SDL_GetTicks()<deadline && town_travel_actor) {
        tig_ping();gamelib_ping();modern_map_route_tick();SDL_Delay(1);
        int64_t current_loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
        if(current_loc!=previous_loc && reached) {
            int64_t dx,dy;location_calc_dist_from_screen_center(current_loc,&dx,&dy);
            assert(llabs(dx)<=360 && llabs(dy)<=180);
            camera_checks++;previous_loc=current_loc;
        }
        if(town_travel_next>reached) {
            reached=town_travel_next;printf("Segment %d/40\n",reached);
            AnimID id;AnimRunInfo* run;
            assert(sub_423300(pc,&id) && anim_id_to_run_info(&id,&run));
            assert(run->flags & 0x80000);
            /* Move the viewport away: the next tile crossing must recenter it. */
            location_origin_set(location_make(LOCATION_GET_X(current_loc)+40,LOCATION_GET_Y(current_loc)+40));
        }
    }
    printf("Travel state: reached=%d active=%lld next=%d busy=%d paused=%d current=(%u,%u) target=(%u,%u)\n",
        reached,(long long)town_travel_actor,town_travel_next,sub_423300(pc,NULL),sub_45B300(),
        LOCATION_GET_X(obj_field_int64_get(pc,OBJ_F_LOCATION)),LOCATION_GET_Y(obj_field_int64_get(pc,OBJ_F_LOCATION)),
        LOCATION_GET_X(town_travel_destination),LOCATION_GET_Y(town_travel_destination));
    assert(reached==40 && camera_checks>=39 && !town_travel_actor && !town_travel_points);
    printf("Final location: (%d,%d), paused=%d\n",(int)location_get_x(obj_field_int64_get(pc,OBJ_F_LOCATION)),
        (int)location_get_y(obj_field_int64_get(pc,OBJ_F_LOCATION)),sub_45B300());
    fflush(stdout);
    assert(obj_field_int64_get(pc,OBJ_F_LOCATION)==destination);
    puts("PASS: real map grows past 30 points, exposes all markers and follows 40 real movement segments with camera recentering");
    return 0;
}
