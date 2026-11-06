#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define chdir _chdir
#define access _access
#define R_OK 4
#else
#include <unistd.h>
#endif
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

    /* Reproduce a nearby attacker created after the initiative snapshot. It
     * threatens the PC (or a follower), but has no turn and no tactical card. */
    ObjectList nearby;
    object_list_vicinity(pc,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next) {
        obj_field_handle_set(n->obj,OBJ_F_NPC_COMBAT_FOCUS,OBJ_HANDLE_NULL);
        obj_field_int32_set(n->obj,OBJ_F_NPC_FLAGS,
            obj_field_int32_get(n->obj,OBJ_F_NPC_FLAGS)&~ONF_FIGHTING);
    }
    object_list_destroy(&nearby);
    combat_critter_activate_combat_mode(pc);
    assert(combat_turn_based_is_active() && combat_tactical_enabled());
    sub_423FF0(pc);
    combat_turn_based_whos_turn_set(pc);
    combat_companion_ai_set(false);
    combat_companion_ai_sync();
    int64_t enemy=OBJ_HANDLE_NULL;
    assert(object_create(obj_field_handle_get(allies[0],OBJ_F_PROTOTYPE_HANDLE),
        obj_field_int64_get(pc,OBJ_F_LOCATION),&enemy));
    assert(critter_is_active(enemy));
    obj_field_handle_set(enemy,OBJ_F_NPC_COMBAT_FOCUS,pc);
    obj_field_int32_set(enemy,OBJ_F_NPC_FLAGS,
        obj_field_int32_get(enemy,OBJ_F_NPC_FLAGS)|ONF_FIGHTING);
    assert(ai_is_fighting(enemy));
    assert(combat_can_exit_combat_mode(pc));
    /* Legacy real-time behavior must still reject a nearby active attacker. */
    combat_turn_based_active=false;
    assert(!combat_can_exit_combat_mode(pc));
    combat_turn_based_active=true;
    combat_turn_based_add_critter(enemy);
    int64_t order[128];bool visible=false;
    int ordered=combat_turn_order(order,128);
    for(int i=0;i<ordered;i++)if(order[i]==enemy)visible=true;
    assert(visible && !combat_can_exit_combat_mode(pc));
    obj_field_handle_set(enemy,OBJ_F_NPC_COMBAT_FOCUS,allies[0]);
    assert(!combat_can_exit_combat_mode(pc));
    object_flags_set(enemy,OF_DONTDRAW);
    assert(combat_can_exit_combat_mode(pc));
    object_flags_unset(enemy,OF_DONTDRAW);
    assert(!combat_can_exit_combat_mode(pc));
    obj_field_handle_set(enemy,OBJ_F_NPC_COMBAT_FOCUS,OBJ_HANDLE_NULL);
    obj_field_int32_set(enemy,OBJ_F_NPC_FLAGS,
        obj_field_int32_get(enemy,OBJ_F_NPC_FLAGS)&~ONF_FIGHTING);
    assert(combat_can_exit_combat_mode(pc));
    combat_turn_based_whos_turn_set(pc);
    sub_423FF0(pc);
    for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
    TigMessage exit_click={.type=TIG_MESSAGE_MOUSE};
    exit_click.data.mouse.event=TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP;
    intgame_process_event(&exit_click);
    assert(!combat_turn_based_is_active());
    object_flags_set(enemy,OF_OFF);
    puts("PASS: unlisted threat permits retreat; listed attacker blocks PC and follower; hidden threat does not trap party; real-time unchanged");

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
