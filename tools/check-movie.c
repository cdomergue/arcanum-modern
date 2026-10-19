#undef NDEBUG
#include <assert.h>
#include <tig/tig.h>
#include "ui/modern_movie.h"

static Uint32 SDLCALL interrupt_movie(void* userdata, SDL_TimerID id, Uint32 interval)
{
    (void)id; (void)interval;
    SDL_Event event = {0};
    event.type = userdata ? SDL_EVENT_QUIT : SDL_EVENT_KEY_DOWN;
    if (!userdata) {
        event.key.key = SDLK_ESCAPE;
        event.key.scancode = SDL_SCANCODE_ESCAPE;
        event.key.down = true;
    }
    SDL_PushEvent(&event);
    return 0;
}

int main(int argc, char** argv)
{
    assert(argc == 2);
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, "dummy");
    TigInitInfo init = {0};
    init.width = 800; init.height = 600; init.bpp = 32;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    assert(tig_init(&init) == TIG_OK);
    modern_movie_init();
    SDL_Renderer* renderer;
    assert(tig_video_renderer_get(&renderer) == TIG_OK);
    int width, height; SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer, &width, &height, &mode);
    Uint64 start = SDL_GetTicks();
    assert(tig_movie_play(argv[1], TIG_MOVIE_NO_FINAL_FLIP, 0) == TIG_OK);
    assert(SDL_GetTicks() - start > 1000);
    int after_width, after_height; SDL_RendererLogicalPresentation after_mode;
    SDL_GetRenderLogicalPresentation(renderer, &after_width, &after_height, &after_mode);
    assert(width == after_width && height == after_height && mode == after_mode);
    assert(!tig_movie_is_playing());
    start = SDL_GetTicks();
    assert(SDL_AddTimer(200, interrupt_movie, NULL));
    assert(tig_movie_play(argv[1], 0, 0) == TIG_OK);
    assert(SDL_GetTicks() - start < 2000);
    assert(SDL_AddTimer(200, interrupt_movie, (void*)1));
    assert(tig_movie_play(argv[1], TIG_MOVIE_IGNORE_KEYBOARD, 0) == TIG_OK);
    start = SDL_GetTicks();
    assert(tig_movie_play(argv[1], 0, 0) == TIG_OK);
    assert(SDL_GetTicks() - start < 100);
    TigMessage message;
    bool quit = false;
    while (tig_message_dequeue(&message) == TIG_OK) if (message.type == TIG_MESSAGE_QUIT) quit = true;
    assert(quit);
    tig_exit();
    puts("PASS: Bink playback, renderer restoration, keyboard skip and quit propagation");
    return 0;
}
