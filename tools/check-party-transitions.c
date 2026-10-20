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
#include "ui/follower_ui.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/stat.h"
#include "game/quest.h"
#include "ui/skill_ui.h"
#include "ui/spell_ui.h"
#include "ui/inven_ui.h"
#include "game/map.h"
#include "game/jumppoint.h"
#include "game/teleport.h"
#include "game/obj_pool.h"

static void travel(int64_t actor, int map, int64_t location)
{
    TeleportData jump={.obj=actor,.map=map,.loc=location};
    assert(teleport_do(&jump));
    teleport_ping(0);
}

static int64_t check_follower(ObjectID id)
{
    int64_t hero=player_get_local_pc_obj();
    int64_t follower=obj_pool_perm_lookup(id);
    assert(obj_handle_is_valid(follower));
    assert((obj_field_int32_get(follower,OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT|OF_OFF))==0);
    assert(critter_leader_get(follower)==hero);
    int matches=0;
    for(int i=0;i<obj_arrayfield_length_get(hero,OBJ_F_CRITTER_FOLLOWER_IDX);i++)
        if(obj_arrayfield_handle_get(hero,OBJ_F_CRITTER_FOLLOWER_IDX,i)==follower)matches++;
    assert(matches==1);
    int iter;int64_t obj;int copies=0;
    if(obj_pool_walk_first(&obj,&iter))do {
        if(objid_is_equal(obj_get_id(obj),id))copies++;
    }while(obj_pool_walk_next(&obj,&iter));
    assert(copies==1);
    return follower;
}

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

    assert(map_current_map()==12);
    int64_t inside_loc=obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION);
    JumpPoint out,stairs;
    assert(jumppoint_get(location_make(109,92),&out));
    char* map_name;
    assert(map_get_name(13,&map_name)&&strstr(map_name,"Bates Mansion Lev 2"));
    stairs=(JumpPoint){.dst_map=13,.dst_loc=location_make(100,100),.loc=inside_loc};
    for(int regroup_before=0;regroup_before<=1;regroup_before++) {
        assert(gamelib_load(argv[2]));
        int64_t hero=player_get_local_pc_obj(),allies[8];
        object_flags_unset(hero,OF_OFF);
        assert(modern_followers_get(allies,8)>=3);
        ObjectID sogg_id=obj_get_id(allies[1]);
        travel(hero,out.dst_map,out.dst_loc);
        int64_t sogg=check_follower(sogg_id);
        assert(modern_party_select(sogg));
        int inventory_count=obj_field_int32_get(sogg,OBJ_F_CRITTER_INVENTORY_NUM);
        travel(sogg,12,inside_loc);
        assert(map_current_map()==out.dst_map);
        if(regroup_before)assert(modern_party_set_grouped(sogg,true));
        travel(player_get_local_pc_obj(),12,inside_loc);
        sogg=check_follower(sogg_id);
        if(!regroup_before)assert(modern_party_set_grouped(sogg,true));
        assert(!modern_party_detached(sogg));
        assert(obj_field_int32_get(sogg,OBJ_F_CRITTER_INVENTORY_NUM)==inventory_count);
        travel(player_get_local_pc_obj(),stairs.dst_map,stairs.dst_loc);
        assert(map_current_map()==stairs.dst_map);
        check_follower(sogg_id);
        travel(player_get_local_pc_obj(),12,stairs.loc);
        check_follower(sogg_id);
        travel(player_get_local_pc_obj(),out.dst_map,out.dst_loc);
        sogg=check_follower(sogg_id);
        assert(obj_field_int32_get(sogg,OBJ_F_CRITTER_INVENTORY_NUM)==inventory_count);
        assert(gamelib_save("Slot0095","Party transition test"));
        assert(gamelib_load("Slot0095"));
        check_follower(sogg_id);
    }
    puts("PASS: regroup before/after entry, unique follower, inventory, stairs, exit and save/load");
    return 0;
}
