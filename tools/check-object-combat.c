#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include "game/name.h"
#include "game/gsound.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/intgame.c"

static void check_goal(int64_t actor, int64_t target, int expected)
{
    sub_423FF0(actor);
    assert(combat_input_ready(actor));
    TargetDescriptor td = {0};
    td.obj = target;
    sub_54ED30(&td);
    AnimID id;
    if (!anim_is_current_goal_type(actor, expected, &id)) {
        fprintf(stderr, "FAIL: target type %d, expected goal %d, combat %d, tactical %d\n",
            obj_field_int32_get(target, OBJ_F_TYPE), expected,
            combat_critter_is_combat_mode_active(actor), combat_turn_based_is_active());
        abort();
    }
    sub_423FF0(actor);
}

static void check_preview(int64_t actor, int64_t target)
{
    location_origin_set(obj_field_int64_get(target, OBJ_F_LOCATION));
    TigRect rect;
    object_get_rect(target, 0, &rect);
    for (int y = rect.y; y < rect.y + rect.height; y += 2) {
        for (int x = rect.x; x < rect.x + rect.width; x += 2) {
            TargetDescriptor td;
            if (!sub_552050(x, y, &td) || td.is_loc || td.obj != target) continue;
            tig_mouse_set_position(x, y);
            ModernCombatPreview preview;
            int ap = combat_action_points_get();
            if (!modern_combat_preview(&preview)) continue;
            assert(preview.actor == actor && preview.target == target && preview.kind == 1);
            assert(preview.cost >= combat_attack_cost(actor) && preview.chance == -1);
            assert(combat_action_points_get() == ap);
            return;
        }
    }
    fprintf(stderr, "FAIL: no attack preview for object type %d\n", obj_field_int32_get(target, OBJ_F_TYPE));
    abort();
}

static void check_object_attack_exit(int64_t pc, int64_t companion, int64_t target, bool locked, bool queued)
{
    int64_t party[8];int count=modern_followers_get(party,8);
    combat_critter_activate_combat_mode(pc);
    combat_turn_based_whos_turn_set(companion);
    sub_423FF0(companion);
    if (locked) for (int i=0;i<count;i++) if (party[i]!=pc) {
        ai_target_lock(party[i], target);
        assert(obj_field_handle_get(party[i], OBJ_F_NPC_COMBAT_FOCUS) == target);
    }
    if (queued) {
        assert(anim_goal_attack(companion, target));
        assert(anim_is_current_goal_type(companion, AG_ATTACK, NULL));
    }
    combat_turn_based_whos_turn_set(pc);
    sub_423FF0(pc);
    assert(combat_can_exit_combat_mode(pc));
    intgame_combat_mode_toggle();
    assert(!combat_turn_based_is_active());
    for (int i=0;i<count;i++) if (party[i]!=pc) {
        assert(obj_field_handle_get(party[i], OBJ_F_NPC_COMBAT_FOCUS) == OBJ_HANDLE_NULL);
        assert(!anim_is_current_goal_type(party[i], AG_ATTACK, NULL));
        assert(!ai_is_fighting(party[i]));
        assert((obj_field_int32_get(party[i], OBJ_F_NPC_FLAGS) & ONF_NO_ATTACK) == 0);
    }
    for (int frame = 0; frame < 100; frame++) {
        for (int i=0;i<count;i++) if (party[i]!=pc) ai_process(party[i]);
        tig_ping();gamelib_ping();modern_ui_tick();SDL_Delay(20);
        assert(!combat_turn_based_is_active());
    }
    puts("PASS: object attack cancelled for companion, no combat restart after AI ticks");
}

int main(int argc, char** argv)
{
    assert(argc == 3 && !chdir(argv[1]) && !access(".map-export-sandbox", R_OK));
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    TigInitInfo init = {0};
    init.width = 1280; init.height = 720; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    assert(tig_init(&init) == TIG_OK);
    printf("Isolated object combat test process %ld, directory %s\n", (long)getpid(), argv[1]);
    intgame_set_iso_window_width(init.width); intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    assert(intgame_create_iso_window(&game.iso_window_handle));
    assert(gamelib_init(&game) && gameuilib_init(&game));
    assert(gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear();
    assert(gamelib_load(argv[2]));
    sub_5412D0(); intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc = player_get_local_pc_obj();
    object_flags_unset(pc, OF_OFF);
    int types[] = {OBJ_TYPE_PORTAL, OBJ_TYPE_CONTAINER, OBJ_TYPE_SCENERY};
    int64_t targets[3] = {0};
    ObjectList nearby;
    int64_t loc = obj_field_int64_get(pc, OBJ_F_LOCATION);
    int64_t x = LOCATION_GET_X(loc), y = LOCATION_GET_Y(loc);
    LocRect area = {x - 30, y - 30, x + 30, y + 30};
    object_list_rect(&area, OBJ_TM_PORTAL | OBJ_TM_CONTAINER | OBJ_TM_SCENERY, &nearby);
    for (ObjectNode* node = nearby.head; node; node = node->next) {
        if (object_hp_current(node->obj) <= 0
            || (obj_field_int32_get(node->obj, OBJ_F_FLAGS) & (OF_OFF | OF_CLICK_THROUGH | OF_DESTROYED)) != 0) continue;
        for (int i = 0; i < 3; i++) {
            if (!targets[i] && obj_field_int32_get(node->obj, OBJ_F_TYPE) == types[i]) targets[i] = node->obj;
        }
    }
    object_list_destroy(&nearby);
    assert(targets[0] && targets[1] && targets[2]);
    combat_critter_deactivate_combat_mode(pc);
    SDL_SetModState(SDL_KMOD_NONE);
    for (int i = 0; i < 3; i++) check_goal(pc, targets[i], i == 1 ? AG_USE_CONTAINER : AG_USE_OBJECT);
    assert(sub_4B6C90(false));
    combat_critter_activate_combat_mode(pc);
    assert(!combat_turn_based_is_active());
    for (int i = 0; i < 3; i++) check_goal(pc, targets[i], AG_ATTACK);
    combat_critter_deactivate_combat_mode(pc);
    assert(modern_ui_init());
    combat_tactical_enable();
    combat_critter_activate_combat_mode(pc);
    assert(combat_turn_based_is_active());
    for (int frame = 0; frame < 100; frame++) {
        modern_ui_tick();
        SDL_Delay(10);
    }
    int64_t actor = combat_input_actor();
    assert(actor);
    for (int i = 0; i < 3; i++) {
        check_goal(actor, targets[i], AG_ATTACK);
        int ap = combat_action_points_get();
        combat_check_attack(actor, targets[i]);
        int expected = combat_required_action_points_get();
        sub_557370(actor, targets[i]);
        assert(combat_required_action_points_get() == expected);
        assert(combat_action_points_get() == ap);
        check_preview(actor, targets[i]);
    }
    SDL_SetModState(SDL_KMOD_SHIFT);
    check_goal(actor, targets[1], AG_ATTEMPT_ATTACK);
    SDL_SetModState(SDL_KMOD_ALT);
    check_goal(actor, targets[0], AG_ATTACK);
    SDL_SetModState(SDL_KMOD_NONE);
    int64_t party[8];int count=modern_followers_get(party,8);
    int64_t companion=OBJ_HANDLE_NULL;
    for(int i=0;i<count;i++)if(party[i]!=pc){companion=party[i];break;}
    assert(companion);
    check_object_attack_exit(pc, companion, targets[0], true, true);
    check_object_attack_exit(pc, companion, targets[1], false, true);
    check_object_attack_exit(pc, companion, targets[2], true, false);
    puts("PASS: real door/container/scenery goals, noncombat use, realtime/tactical attack, Shift/Alt, screen picking and read-only AP preview");
    modern_ui_exit();
    gameuilib_exit(); gamelib_exit(); tig_exit();
}
