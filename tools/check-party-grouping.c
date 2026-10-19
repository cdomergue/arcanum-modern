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
#include "../source/src/ui/follower_ui.c"

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

    int64_t pc=player_get_local_pc_obj(),allies[8];
    object_flags_unset(pc,OF_OFF);
    int count=modern_followers_get(allies,8);
    assert(count>=3);


    int original_count=critter_num_followers(pc,false);
    ObjectID detached_id=obj_get_id(allies[0]);
    assert(modern_party_select(allies[0]));
    assert(modern_party_selected()==allies[0] && modern_party_detached(allies[0]));
    assert(critter_pc_leader_get(allies[0])==pc && critter_num_followers(pc,false)==original_count);
    assert(gamelib_save("Slot0097","Party grouping test"));
    assert(modern_party_set_grouped(allies[0],true));
    assert(!modern_party_detached(allies[0]));
    assert(gamelib_load("Slot0097"));
    sub_5412D0();
    pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    int64_t detached=obj_pool_perm_lookup(detached_id);
    assert(detached && modern_party_detached(detached) && !modern_party_selected());
    assert(critter_pc_leader_get(detached)==pc && critter_num_followers(pc,false)==original_count);
    assert(modern_party_select(detached));
    combat_critter_activate_combat_mode(pc);
    assert(combat_turn_based_is_active() && !modern_party_selected());
    assert(!modern_party_set_grouped(detached,true));
    combat_critter_deactivate_combat_mode(pc);
    assert(modern_party_set_grouped(detached,true) && !modern_party_detached(detached));
    puts("PASS: native grouping preserves membership, save/load and combat ownership");
    return 0;
}
