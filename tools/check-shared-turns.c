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

static void (*turn_changed)(int64_t);
static int64_t expected_enemy;
static bool enemy_visited;
static void record_turn(int64_t actor)
{
    if(actor==expected_enemy)enemy_visited=true;
    if(turn_changed)turn_changed(actor);
}

static void settle_camera(void)
{
    modern_ui_tick();SDL_Delay(260);modern_ui_tick();SDL_Delay(510);modern_ui_tick();
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

    int64_t pc=player_get_local_pc_obj(),allies[8];
    object_flags_unset(pc,OF_OFF);
    int count=modern_followers_get(allies,8);
    assert(count>=3);
    assert(combat_turn_based_start());
    int64_t enemy=0;
    for(ObjectNode* node=combat_critter_list.head;node;node=node->next)
        if(!combat_player_controls(node->obj) && critter_is_active(node->obj)){enemy=node->obj;break;}
    assert(enemy);
    int64_t order[]={pc,allies[0],allies[1],enemy,allies[2]};
    ObjectID ids[5];
    object_list_destroy(&combat_critter_list);
    ObjectNode** tail=&combat_critter_list.head;
    for(int i=0;i<5;i++){
        ids[i]=obj_get_id(order[i]);
        ObjectNode* node=object_node_create();node->obj=order[i];node->next=NULL;*tail=node;tail=&node->next;
        sub_424070(order[i],7,true,true);
    }
    shared_turn_count=0;dword_5FC240=combat_critter_list.head;
    combat_turn_based_whos_turn_set(pc);settle_camera();
    assert(combat_shared_turn_available(allies[0]) && combat_shared_turn_available(allies[1]));
    assert(!combat_shared_turn_available(enemy) && !combat_shared_turn_available(allies[2]));
    int pc_points=combat_action_points_get()-2;
    assert(combat_consume_action_points(pc,2));
    assert(combat_shared_turn_select(allies[0]));settle_camera();
    int ally_points=combat_action_points_get()-3;
    assert(combat_consume_action_points(allies[0],3));
    assert(combat_shared_turn_select(pc));settle_camera();
    assert(combat_action_points_get()==pc_points);
    assert(gamelib_save("Slot0098","Shared turn test"));
    assert(gamelib_load("Slot0098"));sub_5412D0();
    pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    for(int i=0;i<5;i++)order[i]=obj_pool_perm_lookup(ids[i]);
    settle_camera();
    assert(combat_input_actor()==pc && combat_action_points_get()==pc_points);
    assert(combat_shared_turn_available(order[1]) && !combat_shared_turn_available(order[4]));
    assert(combat_shared_turn_select(order[1]));settle_camera();
    assert(combat_action_points_get()==ally_points);
    assert(combat_input_end_turn(order[1]));settle_camera();
    assert(combat_input_actor()==pc && combat_action_points_get()==pc_points);
    assert(!combat_shared_turn_available(order[1]));
    assert(combat_input_end_turn(pc));settle_camera();
    assert(combat_input_actor()==order[2]);
    assert(!combat_shared_turn_available(pc) && !combat_shared_turn_available(order[1]));
    expected_enemy=order[3];turn_changed=combat_callbacks.field_10;combat_callbacks.field_10=record_turn;
    assert(combat_input_end_turn(order[2]));
    assert(enemy_visited);
    puts("PASS: native companion switches, independent AP, full save/load, ended actors and enemy boundary");
    return 0;
}
