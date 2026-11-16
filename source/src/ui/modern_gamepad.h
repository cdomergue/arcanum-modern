#ifndef ARCANUM_UI_MODERN_GAMEPAD_H_
#define ARCANUM_UI_MODERN_GAMEPAD_H_

#include <SDL3/SDL.h>

typedef struct ModernPadVector { float x, y, length; } ModernPadVector;
typedef struct ModernPadState {
    SDL_Gamepad* device;
    SDL_JoystickID id;
    Uint32 held, pressed, released, blocked;
    ModernPadVector left, right;
    bool left_trigger, right_trigger;
    bool initialized, active, focused, neutral;
    bool enabled, rumble;
    int deadzone, cursor_speed;
} ModernPadState;

void modern_pad_init(ModernPadState* pad);
void modern_pad_exit(ModernPadState* pad);
bool modern_pad_event(ModernPadState* pad, const SDL_Event* event);
void modern_pad_sample(ModernPadState* pad, bool focused);
void modern_pad_clear_edges(ModernPadState* pad);
void modern_pad_barrier(ModernPadState* pad);
void modern_pad_rumble(ModernPadState* pad, float strength);
void modern_pad_menu_pulse(ModernPadState* pad);
bool modern_pad_movie_skip(ModernPadState* pad, bool focused);
void modern_pad_log(const char* event, int context, int action);
ModernPadVector modern_pad_vector(Sint16 x, Sint16 y, int deadzone);
int modern_pad_sector(ModernPadVector vector, int count);
int modern_pad_focus(const SDL_FRect* rects, int count, int current, int x, int y);
const char* modern_pad_button_name(ModernPadState* pad, SDL_GamepadButton button);

#define PAD_BUTTON(button) (1u << (button))

#endif
