#ifndef ARCANUM_MODERN_MAP_GENERATOR_H
#define ARCANUM_MODERN_MAP_GENERATOR_H

#include <SDL3/SDL.h>

typedef struct ModernMapProgress {
    unsigned long long done, total;
    int stage;
} ModernMapProgress;

char* modern_maps_executable(const char* argv0);
bool modern_maps_ready(void);
SDL_Process* modern_maps_start(const char* executable);
void modern_maps_progress(ModernMapProgress* progress);
int modern_maps_generate(const char* game, int tile_limit);

#endif
