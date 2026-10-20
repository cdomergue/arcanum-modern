#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/background.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/location.h"
#include "game/level.h"
#include "game/modern_compat.h"
#include "game/name.h"
#include "game/object.h"
#include "game/player.h"
#include "game/portrait.h"
#include "game/stat.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"

#include "../source/src/ui/mainmenu_ui.c"

int main(int argc, char** argv)
{
    if ((argc != 3 && argc != 4) || chdir(argv[1]) || access(".map-export-sandbox", R_OK)) return 2;
    setvbuf(stdout, NULL, _IOLBF, 0);
    int races = atoi(argv[2]);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = 1280; init.height = 800; init.bpp = 32;
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
    gamelib_splash_clear();
    assert(map_preprocess_mobile("ShopMap"));
    assert(map_open("maps\\ShopMap", "uap-check-state", false));
    PlayerCreateInfo pc;
    player_create_info_init(&pc);
    pc.basic_prototype = 16066;
    pc.loc = location_make(100, 100);
    assert(player_obj_create_player(&pc));
    int64_t hero = player_get_local_pc_obj();
    assert(hero);
    for (int race = 0; race < races; race++) {
        assert(stat_base_get(hero, STAT_RACE) == race);
        int portrait, background;
        assert(portrait_find_first(hero, &portrait));
        assert(background_find_first(hero, &background));
        int backgrounds = 0;
        do {
            background_set(hero, background, background_get_description(background));
            for (int stat = 0; stat < STAT_COUNT; stat++) (void)stat_level_get(hero, stat);
            background_clear(hero);
            assert(++backgrounds < 1000);
        } while (background_find_next(hero, &background));
        for (int rotation = 0; rotation < 8; rotation++) {
            TigArtFrameData frame;
            tig_art_id_t art = tig_art_id_rotation_set(obj_field_int32_get(hero, OBJ_F_CURRENT_AID), rotation);
            assert(tig_art_frame_data(art, &frame) == TIG_OK);
            assert(frame.width > 0 && frame.height > 0);
        }
        printf("PASS: race %d, portrait %d, background %d, character art\n", race, portrait, background);
        assert(mainmenu_ui_new_char_next_race(hero));
    }
    assert(stat_base_get(hero, STAT_RACE) == RACE_HUMAN);
    const int cursors[] = {0, 21, 352, 353, 818, 819, 820};
    SDL_Surface* canvas = SDL_CreateSurface(160, 160, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(canvas);
    assert(renderer);
    tig_mouse_set_active(true);
    tig_mouse_show();
    for (size_t c = 0; c < SDL_arraysize(cursors); c++) {
        tig_art_id_t art;
        assert(tig_art_interface_id_create(cursors[c], 0, 0, 0, &art) == TIG_OK);
        assert(tig_mouse_cursor_set_art_id(art) == TIG_OK);
        SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
        SDL_RenderClear(renderer);
        tig_mouse_display_native(renderer, 80, 80, 1);
        SDL_RenderPresent(renderer);
        int visible = 0;
        for (int y = 0; y < canvas->h; y++) {
            Uint32* pixels = (Uint32*)((char*)canvas->pixels + y * canvas->pitch);
            for (int x = 0; x < canvas->w; x++) if ((pixels[x] & 0xffffff) != 0xff00ff) visible++;
        }
        assert(visible > 0 && visible < 160 * 160);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(canvas);
    sub_549A80();
    assert(mainmenu_ui_pregen_char_cnt == (races == 11 ? 16 : 13));
    for (int i = 1; i < mainmenu_ui_pregen_char_cnt; i++) {
        int64_t pregen = sub_4685A0(BP_GENERIC_PC + i);
        assert(pregen != OBJ_HANDLE_NULL);
        assert(tig_art_exists(obj_field_int32_get(pregen, OBJ_F_CURRENT_AID)) == TIG_OK);
    }
    assert(mainmenu_ui_new_char_set_gender(hero, GENDER_FEMALE));
    int female_races = 0;
    do {
        int portrait, background;
        assert(portrait_find_first(hero, &portrait));
        assert(background_find_first(hero, &background));
        assert(mainmenu_ui_new_char_next_race(hero));
        assert(++female_races <= races);
    } while (stat_base_get(hero, STAT_RACE) != RACE_HUMAN);
    assert(female_races == (races == 11 ? 5 : 4));
    for (int i = 0; i < female_races; i++) assert(mainmenu_ui_new_char_prev_race(hero));
    assert(stat_base_get(hero, STAT_RACE) == RACE_HUMAN);
    const ModernCompatibility* options = modern_compat_get();
    int flags = argc == 4 ? atoi(argv[3]) : 0;
    assert(options->races == (races == 11));
    assert(options->level_limit == ((flags & 1) ? 101 : 51));
    assert(options->master_educator == !!(flags & 2));
    assert(options->no_combat_music == !!(flags & 4));
    assert(options->no_xp_per_hit == !!(flags & 8));
    assert(stat_level_max(hero, STAT_LEVEL) == options->level_limit);
    level_set(hero, (flags & 1) ? 75 : 25);
    assert(stat_base_get(hero, STAT_LEVEL) == ((flags & 1) ? 75 : 25));
    assert(level_experience_points_to_next_level(hero) > 0);
    printf("PASS: female race cycle, all backgrounds, eight rotations, cursor, engine options %d and leveling\n", flags);
    return 0;
}
