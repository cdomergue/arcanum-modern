#ifndef ARCANUM_UI_MODERN_GRAPHICS_H_
#define ARCANUM_UI_MODERN_GRAPHICS_H_

#include <SDL3/SDL.h>

typedef struct ModernDisplaySettings {
    int mode; /* 0: windowed, 1: desktop fullscreen, 2: exclusive fullscreen. */
    int width, height;
} ModernDisplaySettings;

typedef struct ModernGraphics {
    ModernDisplaySettings selected;
    ModernDisplaySettings previous;
    SDL_Point sizes[128];
    int count, index;
    Uint64 deadline;
    char status[192];
} ModernGraphics;

extern ModernGraphics modern_graphics;
void modern_graphics_init(SDL_Window* window);
void modern_graphics_begin(void);
void modern_graphics_mode(int direction);
void modern_graphics_resolution(int direction);
void modern_graphics_apply(void);
void modern_graphics_confirm(void);
void modern_graphics_cancel(void);
void modern_graphics_tick(void);

#endif
