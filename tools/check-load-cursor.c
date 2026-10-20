#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"

static SDL_Window* mouse_focus;
static SDL_Window* test_mouse_focus(void) { return mouse_focus; }
#define SDL_GetMouseFocus test_mouse_focus
#include "../source/first_party/tig/src/message.c"
#undef SDL_GetMouseFocus

static int cursor_pixels(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);
    tig_mouse_display_native(renderer, 80, 80, 1);
    SDL_Surface* pixels = SDL_RenderReadPixels(renderer, NULL);
    assert(pixels);
    SDL_Surface* rgb = SDL_ConvertSurface(pixels, SDL_PIXELFORMAT_XRGB8888);
    assert(rgb);
    int count = 0;
    for (int y = 0; y < rgb->h; y++) {
        Uint32* row = (Uint32*)((char*)rgb->pixels + y * rgb->pitch);
        for (int x = 0; x < rgb->w; x++) if ((row[x] & 0xffffff) != 0xff00ff) count++;
    }
    SDL_DestroySurface(rgb);
    SDL_DestroySurface(pixels);
    return count;
}

int main(int argc, char** argv)
{
    if (argc != 3 || chdir(argv[1]) || access(".map-export-sandbox", R_OK)) return 2;
    setvbuf(stdout, NULL, _IOLBF, 0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    TigInitInfo init = {0};
    init.width = 800; init.height = 600; init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    assert(tig_init(&init) == TIG_OK);
    intgame_set_iso_window_width(init.width); intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    assert(intgame_create_iso_window(&game.iso_window_handle) && gamelib_init(&game));
    assert(gameuilib_init(&game) && gamelib_mod_load("Arcanum") && gameuilib_mod_load());
    gamelib_splash_clear();
    assert(gamelib_load(argv[2]));
    intgame_mode_set(INTGAME_MODE_MAIN);
    intgame_refresh_cursor();
    SDL_Renderer* renderer;
    SDL_Window* window;
    assert(tig_video_renderer_get(&renderer) == TIG_OK);
    assert(tig_video_window_get(&window) == TIG_OK);
    SDL_SetRenderLogicalPresentation(renderer, 800, 600, SDL_LOGICAL_PRESENTATION_DISABLED);
    tig_mouse_show();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
    mouse_focus = window;
    tig_set_active(false);
    assert(cursor_pixels(renderer) == 0);
    tig_message_ping();
    printf("After load with consumed enter event: active=%d, cursor pixels=%d\n",
        tig_get_active(), cursor_pixels(renderer));
    assert(tig_get_active() && cursor_pixels(renderer) > 0);

    mouse_focus = NULL;
    tig_message_ping();
    assert(!tig_get_active() && cursor_pixels(renderer) == 0);
    mouse_focus = window;
    tig_mouse_hide();
    tig_message_ping();
    assert(tig_get_active() && cursor_pixels(renderer) == 0);
    tig_mouse_show();
    assert(cursor_pixels(renderer) > 0);
    puts("PASS: loaded save cursor recovers focus; leaving and explicit hiding remain respected");
    gameuilib_exit(); gamelib_exit(); tig_exit();
    return 0;
}
