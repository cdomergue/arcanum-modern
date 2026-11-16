#undef NDEBUG
#include <assert.h>
#include <tig/tig.h>
#include "ui/modern_movie.h"
#include "ui/modern_ui.h"
/* Headless test focus only; production always checks the actual SDL window. */
static SDL_Window* check_movie_focus(void);
#define SDL_GetKeyboardFocus check_movie_focus
#include "../source/src/ui/modern_movie.c"
#undef SDL_GetKeyboardFocus
static SDL_Window* check_movie_focus(void) {SDL_Window* result=NULL;tig_video_window_get(&result);return result;}
bool modern_tutorial_skip_movie(const char* path) {(void)path;return false;}
static int barriers;
void modern_ui_gamepad_barrier(void) {barriers++;}

static SDL_Joystick* test_stick;
static SDL_GamepadButton test_button;
static Uint32 SDLCALL press_pad(void* data,SDL_TimerID id,Uint32 interval)
{
    (void)id;(void)interval;
    assert(SDL_SetJoystickVirtualButton(test_stick,test_button,data!=NULL));return 0;
}

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
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0x1209/0xa11c");
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
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,"1");
    assert(SDL_InitSubSystem(SDL_INIT_GAMEPAD));
    SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
    desc.vendor_id=0x1209;desc.product_id=0xa11c;
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;desc.name="Movie skip regression";
    SDL_JoystickID device=SDL_AttachVirtualJoystick(&desc);assert(device);
    test_stick=SDL_OpenJoystick(device);assert(test_stick);
    const SDL_GamepadButton buttons[]={SDL_GAMEPAD_BUTTON_SOUTH,SDL_GAMEPAD_BUTTON_EAST,SDL_GAMEPAD_BUTTON_START};
    for(int i=0;i<3;i++) {
        test_button=buttons[i];int before=barriers;start=SDL_GetTicks();
        assert(SDL_AddTimer(200,press_pad,(void*)1));assert(SDL_AddTimer(600,press_pad,NULL));
        assert(tig_movie_play(argv[1],0,0)==TIG_OK);
        assert(SDL_GetTicks()-start>=550 && SDL_GetTicks()-start<2000 && barriers==before+1);
        assert(!tig_movie_is_playing());
    }
    /* Holding a button from a previous screen must not skip the next movie. */
    test_button=SDL_GAMEPAD_BUTTON_START;
    assert(SDL_SetJoystickVirtualButton(test_stick,test_button,true));SDL_UpdateJoysticks();
    assert(SDL_AddTimer(400,press_pad,NULL));assert(SDL_AddTimer(700,press_pad,(void*)1));assert(SDL_AddTimer(1000,press_pad,NULL));
    start=SDL_GetTicks();assert(tig_movie_play(argv[1],0,0)==TIG_OK);
    assert(SDL_GetTicks()-start>=950 && SDL_GetTicks()-start<2000);
    SDL_CloseJoystick(test_stick);assert(SDL_DetachVirtualJoystick(device));SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
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
    start = SDL_GetTicks();
    assert(SDL_AddTimer(200, interrupt_movie, NULL));
    assert(tig_movie_play(argv[1], 0, 0) == TIG_OK);
    assert(SDL_GetTicks() - start >= 150);
    assert(SDL_GetTicks() - start < 2000);
    tig_exit();
    puts("PASS: Bink playback, A/B/Start skip before UI initialization, held-button barrier, renderer restoration, keyboard skip and quit propagation");
    return 0;
}
