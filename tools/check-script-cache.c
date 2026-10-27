/* Link instead of main.c and script.c. Use an isolated, marked game copy.
 * Usage: check-script-cache GAME SAVE_SLOT [flags|lifetime] */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/modern_ui.h"
#include "../source/src/game/script.c"
void sub_549A70(void);
void sub_5412D0(void);

int main(int argc, char** argv)
{
    if ((argc != 3 && argc != 4) || chdir(argv[1]) || access(".map-export-sandbox", R_OK)) return 2;
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = 1360; init.height = 900; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    assert(tig_init(&init) == TIG_OK);
    intgame_set_iso_window_width(init.width); intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    assert(intgame_create_iso_window(&game.iso_window_handle) && gamelib_init(&game));
    sub_549A70();
    assert(gameuilib_init(&game) && gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear(); assert(modern_ui_init());
    printf("Test process %ld, isolated directory %s\n", (long)getpid(), argv[1]);
    assert(gamelib_load(argv[2])); sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc = player_get_local_pc_obj();
    object_flags_unset(pc, OF_OFF);
    location_origin_set(obj_field_int64_get(pc, OBJ_F_LOCATION));
    iso_interface_refresh();

    if (argc == 4 && !strcmp(argv[3], "lifetime")) {
        /* Self-removal clears Script.num while its code is still locked. */
        int index = cache_find(99999);
        cache_remove(index);
        ScriptFile* file;
        assert(script_file_create(&file));
        file->flags = SF_AUTO_REMOVING;
        file->num_entries = file->max_entries = 1;
        file->entries = CALLOC(1, sizeof(ScriptCondition));
        file->entries[0].type = SCT_TRUE;
        file->entries[0].action.type = SAT_RETURN_AND_RUN_DEFAULT;
        script_cache_entries[index] = (ScriptCacheEntry){.script_id = 99999, .file = file};
        Script scr = {.num = 99999};
        ScriptInvocation invocation = {0};
        invocation.script = &scr;
        invocation.attachee_obj = pc;
        invocation.triggerer_obj = pc;
        invocation.attachment_point = SAP_USE;
        assert(script_execute(&invocation));
        assert(scr.num == 0);
        assert(script_cache_entries[index].ref_count == 0);
        for (int i = 0; i < MAX_CACHE_ENTRIES; i++) assert(script_cache_entries[i].ref_count >= 0);
        puts("PASS: auto-removing script releases its original cache entry");
        return 0;
    }

    int ids[500], count = 0;
    char path[TIG_MAX_PATH];
    for (int id = 1; id < 40000 && count < 500; id++) {
        if (script_name_build_scr_name(id, path, sizeof(path))) ids[count++] = id;
    }
    assert(count == 500);
    /* Hold a live script across cache churn: it must never be evicted. */
    ScriptFile* pinned = script_lock(ids[0]);
    assert(pinned && script_lock(ids[0]) == pinned);
    script_unlock(ids[0]);
    for (int pass = 0; pass < 3; pass++) {
        for (int i = 1; i < count; i++) {
            Script scr = {.num = ids[i]};
            ScriptFlags flags;
            assert(script_flags(&scr, &flags));
            if (i % 50 == 0) printf("Pass %d: %d distinct scripts read\n", pass + 1, i);
        }
        assert(script_lock(ids[0]) == pinned);
        script_unlock(ids[0]);
    }
    script_unlock(ids[0]);
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) assert(script_cache_entries[i].ref_count == 0);
    /* The editor bypasses the runtime cache entirely. */
    script_editor = true;
    assert(script_lock(ids[0]) == NULL);
    script_unlock(ids[0]);
    script_editor = false;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) assert(script_cache_entries[i].ref_count == 0);
    assert(gamelib_save("CacheTest", "Script cache regression"));
    assert(gamelib_load("CacheTest"));
    puts("PASS: 500 real scripts, three passes, pinned/nested locks, editor bypass and save reload");
    return 0;
}
