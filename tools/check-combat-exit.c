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
#include "../source/src/game/combat.c"

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

    for(int i=0;i<count;i++) {
        combat_critter_activate_combat_mode(pc);
        assert(combat_turn_based_is_active());
        combat_turn_based_whos_turn_set(allies[i]);
        object_flags_unset(allies[i],OF_OFF);
        obj_field_int32_set(allies[i],OBJ_F_CRITTER_FLAGS,
            obj_field_int32_get(allies[i],OBJ_F_CRITTER_FLAGS)&~OCF_COMBAT_MODE_ACTIVE);
        sub_424070(allies[i],3,0,1);
        assert(combat_input_actor()==allies[i]);
        for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
        assert(!modern_combat_input_blocked());
        assert(combat_can_exit_combat_mode(pc));
        TigMessage click={.type=TIG_MESSAGE_MOUSE};
        click.data.mouse.event=TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP;
        intgame_process_event(&click);
        assert(!combat_turn_based_is_active());
    }
    puts("PASS: right-click exits completed combat from every idle companion");
    return 0;
}
