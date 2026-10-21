#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/combat.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/modern_autosave.h"
#include "game/name.h"
#include "game/obj.h"
#include "game/player.h"
#include "ui/gameuilib.h"
#include "ui/follower_ui.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"

static unsigned long long manual_fingerprint(void)
{
    TigFileList files;
    unsigned long long hash = 1469598103934665603ULL;
    tig_file_list_create(&files, "save\\Slot0003*");
    assert(files.count == 4);
    for (unsigned int index = 0; index < files.count; index++) {
        char path[TIG_MAX_PATH];
        unsigned char buffer[4096];
        size_t count;
        snprintf(path, sizeof(path), "save\\%s", files.entries[index].path);
        TigFile* stream = tig_file_fopen(path, "rb");
        assert(stream != NULL);
        while ((count = tig_file_fread(buffer, 1, sizeof(buffer), stream)) > 0) {
            for (size_t byte = 0; byte < count; byte++) hash = (hash ^ buffer[byte]) * 1099511628211ULL;
        }
        assert(!tig_file_ferror(stream));
        assert(tig_file_fclose(stream) == 0);
    }
    tig_file_list_destroy(&files);
    return hash;
}

static int completed_autosaves(void)
{
    GameSaveList saves;
    int count = 0;
    gamelib_savelist_create(&saves);
    for (unsigned int index = 0; index < saves.count; index++) {
        if (!modern_autosave_is_slot(saves.names[index])) continue;
        char path[TIG_MAX_PATH];
        const char* extensions[] = { ".tfaf", ".tfai", ".bmp" };
        for (int extension = 0; extension < 3; extension++) {
            snprintf(path, sizeof(path), "save\\%.8s%s", saves.names[index], extensions[extension]);
            assert(tig_file_exists(path, NULL));
        }
        GameSaveInfo info;
        memset(&info, 0, sizeof(info));
        assert(gamelib_saveinfo_load(saves.names[index], &info));
        gamelib_saveinfo_exit(&info);
        count++;
    }
    gamelib_savelist_destroy(&saves);
    return count;
}

int main(int argc, char** argv)
{
    if (argc != 2 || chdir(argv[1]) || access(".map-export-sandbox", R_OK)) return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = { 0 };
    init.width = highres_config_get()->width;
    init.height = highres_config_get()->height;
    init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    if (tig_init(&init) != TIG_OK) return 3;
    intgame_set_iso_window_width(init.width);
    intgame_set_iso_window_height(init.height);
    GameInitInfo game = { 0 };
    if (!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game)) return 4;
    if (!gameuilib_init(&game) || !gamelib_mod_load("Arcanum") || !gameuilib_mod_load()) return 5;
    gamelib_splash_clear();
    if (!modern_ui_init() || !gamelib_load("Slot0003")) return 6;
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc = player_get_local_pc_obj();
    object_flags_unset(pc, OF_OFF);
    combat_critter_deactivate_combat_mode(pc);
    modern_autosave_reset();
    unsigned long long manual = manual_fingerprint();
    for (int index = 0; index < 4; index++) {
        modern_autosave_before_map_change();
        assert(completed_autosaves() > 0 && completed_autosaves() <= 3);
        assert(manual_fingerprint() == manual);
        SDL_Delay(1100);
    }
    assert(completed_autosaves() == 3);
    char previous[256];
    strcpy(previous, gamelib_last_save_name());
    assert(modern_autosave_is_slot(previous));
    combat_critter_activate_combat_mode(pc);
    assert(combat_turn_based_is_active());
    assert(strcmp(gamelib_last_save_name(), previous) != 0);
    char before_combat[9];
    memcpy(before_combat, gamelib_last_save_name(), 8);
    before_combat[8] = '\0';
    strcpy(previous, gamelib_last_save_name());
    combat_critter_activate_combat_mode(pc);
    modern_autosave_before_map_change();
    assert(strcmp(gamelib_last_save_name(), previous) == 0);
    combat_critter_deactivate_combat_mode(pc);
    assert(!combat_turn_based_is_active());
    modern_autosave_tick();
    assert(strcmp(gamelib_last_save_name(), previous) == 0);
    SDL_Delay(1100);
    modern_autosave_tick();
    assert(strcmp(gamelib_last_save_name(), previous) != 0);
    assert(completed_autosaves() == 3);
    char latest[9];
    memcpy(latest, gamelib_last_save_name(), 8);
    latest[8] = '\0';
    assert(modern_autosave_is_slot(latest));
    assert(gamelib_load(latest));
    sub_5412D0();
    assert(player_get_local_pc_obj() != OBJ_HANDLE_NULL);
    assert(manual_fingerprint() == manual);
    assert(gamelib_load(before_combat));
    sub_5412D0();
    assert(!combat_turn_based_is_active());
    assert(!combat_critter_is_combat_mode_active(player_get_local_pc_obj()));
    assert(manual_fingerprint() == manual);
    object_flags_unset(player_get_local_pc_obj(), OF_OFF);
    int64_t companions[8];
    assert(modern_followers_get(companions, 8) > 0);
    SDL_Delay(1100);
    strcpy(previous, gamelib_last_save_name());
    combat_critter_activate_combat_mode(companions[0]);
    assert(combat_turn_based_is_active());
    assert(strcmp(gamelib_last_save_name(), previous) != 0);
    memcpy(before_combat, gamelib_last_save_name(), 8);
    before_combat[8] = '\0';
    strcpy(previous, gamelib_last_save_name());
    combat_critter_activate_combat_mode(companions[0]);
    assert(strcmp(gamelib_last_save_name(), previous) == 0);
    assert(gamelib_load(before_combat));
    sub_5412D0();
    assert(!combat_turn_based_is_active());
    assert(!combat_critter_is_combat_mode_active(player_get_local_pc_obj()));
    assert(manual_fingerprint() == manual);
    printf("PASS: player and companion pre-combat saves reload before initiative; 3 complete autosaves; pre-zone save, combat guard, deferred combat-end hook, latest %s loaded; Slot0003 unchanged (%016llx)\n", latest, manual);
    return 0;
}
