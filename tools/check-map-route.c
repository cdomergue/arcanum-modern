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
    int64_t from=obj_field_int64_get(pc,OBJ_F_LOCATION),destination=0,points[TOWN_ROUTE_LIMIT];
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
            if(town_route_plan(&search,from,candidate,TOWN_ROUTE_LIMIT,points)>1)destination=candidate;
        }
    }
    assert(destination && !anim_preview_route_segment(pc,from,destination));
    assert(sub_5643E0(&coords));
    WmapRoute* route=&wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN];
    assert(route->length>1 && route->length<=TOWN_ROUTE_LIMIT);
    int64_t previous=from;
    for(int i=0;i<route->length;i++) {
        assert(anim_preview_route_segment(pc,previous,route->waypoints[i].loc));
        previous=route->waypoints[i].loc;
    }
    assert(previous==destination);
    assert(critter_is_active(pc) && !combat_turn_based_is_active());
    printf("Following %d verified waypoints from (%d,%d) to (%d,%d)\n",route->length,
        (int)LOCATION_GET_X(from),(int)LOCATION_GET_Y(from),(int)LOCATION_GET_X(destination),(int)LOCATION_GET_Y(destination));
    fflush(stdout);
    modern_map_action(3);
    assert(!wmap_ui_is_created());
    Uint64 deadline=SDL_GetTicks()+60000;
    while(SDL_GetTicks()<deadline && obj_field_int64_get(pc,OBJ_F_LOCATION)!=destination) {
        tig_ping();gamelib_ping();SDL_Delay(1);
    }
    printf("Final location: (%d,%d), paused=%d\n",(int)location_get_x(obj_field_int64_get(pc,OBJ_F_LOCATION)),
        (int)location_get_y(obj_field_int64_get(pc,OBJ_F_LOCATION)),sub_45B300());
    fflush(stdout);
    assert(obj_field_int64_get(pc,OBJ_F_LOCATION)==destination);
    puts("PASS: real map rejects direct click, accepts automatic waypoints and walks to destination");
    return 0;
}
