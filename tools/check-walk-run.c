#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "game/name.h"
#include "game/gsound.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/intgame.c"

static void wait_for_animation(int64_t actor, int expected)
{
    Uint64 deadline = SDL_GetTicks() + 3000;
    while (SDL_GetTicks() < deadline) {
        tig_ping();
        gamelib_ping();
        if (tig_art_id_anim_get(obj_field_int32_get(actor, OBJ_F_CURRENT_AID)) == expected) return;
        SDL_Delay(5);
    }
    fprintf(stderr, "FAIL: expected animation %d, got %d\n", expected,
        tig_art_id_anim_get(obj_field_int32_get(actor, OBJ_F_CURRENT_AID)));
    AnimID id;
    AnimRunInfo* info;
    if (sub_423300(actor, &id) && anim_id_to_run_info(&id, &info)) {
        fprintf(stderr, "goal=%d current=%d state=%d flags=%x\n",
            info->goals[0].type, info->current_goal, info->current_state, info->flags);
    }
    abort();
}

static void click_ground(int64_t actor)
{
    int64_t from = obj_field_int64_get(actor, OBJ_F_LOCATION);
    location_origin_set(from);
    TigMessage msg = {0};
    msg.type = TIG_MESSAGE_MOUSE;
    for (int y = 120; y < 550; y += 20) {
        for (int x = 160; x < 1120; x += 20) {
            TargetDescriptor td;
            msg.data.mouse.x = x; msg.data.mouse.y = y;
            if (!sub_5517A0(&msg) || !sub_552050(x, y, &td) || !td.is_loc
                || location_dist(from, td.loc) < 2 || location_dist(from, td.loc) > 12
                || !anim_preview_route_segment(actor, from, td.loc)) continue;
            msg.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;
            intgame_process_event(&msg);
            msg.data.mouse.event = TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
            intgame_process_event(&msg);
            return;
        }
    }
    fputs("FAIL: no reachable ground target in the viewport\n", stderr);
    abort();
}

int main(int argc, char** argv)
{
    assert(argc == 3 && !chdir(argv[1]) && !access(".map-export-sandbox", R_OK));
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    TigInitInfo init = {0};
    init.width = 1280; init.height = 720; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    assert(tig_init(&init) == TIG_OK);
    intgame_set_iso_window_width(init.width); intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    assert(intgame_create_iso_window(&game.iso_window_handle));
    assert(gamelib_init(&game) && gameuilib_init(&game));
    assert(gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear();
    assert(modern_ui_init());
    assert(gamelib_load(argv[2]));
    sub_5412D0(); intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc = player_get_local_pc_obj();
    object_flags_unset(pc, OF_OFF);
    combat_critter_deactivate_combat_mode(pc);
    sub_423FF0(pc);
    SDL_SetModState(SDL_KMOD_NONE);
    assert(!sub_45B300());
    set_always_run(true);
    click_ground(pc);
    set_always_run(false);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_WALK);

    set_always_run(true);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_RUN);
    set_always_run(false);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_WALK);

    set_always_run(true);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_RUN);
    SDL_SetModState(SDL_KMOD_CTRL);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_WALK);
    SDL_Delay(200);
    assert(!get_always_run(pc));
    SDL_SetModState(SDL_KMOD_NONE);
    click_ground(pc);
    wait_for_animation(pc, TIG_ART_ANIM_RUN);

    puts("PASS: loaded save, pending run cancelled, active run/walk transitions and Ctrl hold/release produce the correct animations");
    modern_ui_exit();
    gameuilib_exit(); gamelib_exit(); tig_exit();
}
