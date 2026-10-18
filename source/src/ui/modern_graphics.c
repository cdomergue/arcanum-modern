#include "ui/modern_graphics.h"

#include <stdio.h>
#include <string.h>

ModernGraphics modern_graphics;
static SDL_Window* display_window;

static ModernDisplaySettings current_settings(void)
{
    ModernDisplaySettings settings = {0};
    SDL_GetWindowSize(display_window, &settings.width, &settings.height);
    if (SDL_GetWindowFlags(display_window) & SDL_WINDOW_FULLSCREEN) {
        const SDL_DisplayMode* mode = SDL_GetWindowFullscreenMode(display_window);
        settings.mode = mode ? 2 : 1;
        if (mode) { settings.width = mode->w; settings.height = mode->h; }
    }
    return settings;
}

static void add_size(int width, int height)
{
    ModernGraphics* g = &modern_graphics;
    if (width < 1024 || height < 600 || g->count >= 128) return;
    for (int i = 0; i < g->count; i++) {
        if (g->sizes[i].x == width && g->sizes[i].y == height) return;
    }
    g->sizes[g->count++] = (SDL_Point){width, height};
}

static void enumerate_sizes(void)
{
    ModernGraphics* g = &modern_graphics;
    SDL_DisplayID display = SDL_GetDisplayForWindow(display_window);
    g->count = 0;
    if (g->selected.mode == 2) {
        int count = 0;
        SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes(display, &count);
        for (int i = 0; modes && i < count; i++) add_size(modes[i]->w, modes[i]->h);
        SDL_free(modes);
    } else {
        const SDL_DisplayMode* desktop = SDL_GetDesktopDisplayMode(display);
        if (g->selected.mode == 0) {
            const SDL_Point common[] = {{1024,768},{1280,720},{1280,800},{1280,1024},
                {1600,900},{1920,1080},{1920,1200},{2560,1440},{3440,1440},{3840,2160}};
            for (int i = 0; i < (int)SDL_arraysize(common); i++) {
                if (!desktop || (common[i].x <= desktop->w && common[i].y <= desktop->h)) {
                    add_size(common[i].x, common[i].y);
                }
            }
        }
        if (desktop) add_size(desktop->w, desktop->h);
    }
    /* Sort from smallest to largest, independently of refresh-rate duplicates. */
    for (int i = 1; i < g->count; i++) {
        SDL_Point size = g->sizes[i];
        int j = i;
        while (j > 0 && (g->sizes[j-1].x > size.x
            || (g->sizes[j-1].x == size.x && g->sizes[j-1].y > size.y))) {
            g->sizes[j] = g->sizes[j-1]; j--;
        }
        g->sizes[j] = size;
    }
    g->index = 0;
    for (int i = 0; i < g->count; i++) {
        if (g->sizes[i].x == g->selected.width && g->sizes[i].y == g->selected.height) g->index = i;
    }
    if (g->count) {
        g->selected.width = g->sizes[g->index].x;
        g->selected.height = g->sizes[g->index].y;
    }
}

static bool set_display(ModernDisplaySettings settings)
{
    SDL_DisplayMode mode;
    if (settings.mode < 0 || settings.mode > 2 || settings.width < 1024
        || settings.height < 600 || settings.width > 16384 || settings.height > 16384) return false;
    if (settings.mode == 2) {
        if (!SDL_GetClosestFullscreenDisplayMode(SDL_GetDisplayForWindow(display_window),
                settings.width, settings.height, 0, true, &mode)
            || mode.w != settings.width || mode.h != settings.height) return false;
    }
    if (!SDL_SetWindowFullscreen(display_window, false) || !SDL_SyncWindow(display_window)) return false;
    if (!SDL_SetWindowFullscreenMode(display_window, settings.mode == 2 ? &mode : NULL)) return false;
    if (settings.mode == 0) {
        if (!SDL_SetWindowSize(display_window, settings.width, settings.height)) return false;
    } else if (!SDL_SetWindowFullscreen(display_window, true)) return false;
    if (!SDL_SyncWindow(display_window)) return false;
    ModernDisplaySettings actual = current_settings();
    if (actual.mode != settings.mode) return false;
    if (settings.mode == 0 && (actual.width != settings.width || actual.height != settings.height)) return false;
    return true;
}

void modern_graphics_begin(void)
{
    modern_graphics.selected = current_settings();
    modern_graphics.status[0] = 0;
    enumerate_sizes();
}

void modern_graphics_mode(int direction)
{
    if (modern_graphics.deadline) return;
    modern_graphics.selected.mode = (modern_graphics.selected.mode + direction + 3) % 3;
    modern_graphics.status[0] = 0;
    enumerate_sizes();
}

void modern_graphics_resolution(int direction)
{
    ModernGraphics* g = &modern_graphics;
    if (g->deadline || !g->count || g->selected.mode == 1) return;
    g->index = (g->index + direction + g->count) % g->count;
    g->selected.width = g->sizes[g->index].x;
    g->selected.height = g->sizes[g->index].y;
    g->status[0] = 0;
}

void modern_graphics_cancel(void)
{
    ModernGraphics* g = &modern_graphics;
    if (!g->deadline) return;
    g->deadline = 0;
    bool restored = set_display(g->previous);
    modern_graphics_begin();
    SDL_strlcpy(g->status, restored ? "Réglages précédents rétablis." :
        "Restauration impossible. Choisissez un autre mode.", sizeof(g->status));
}

void modern_graphics_apply(void)
{
    ModernGraphics* g = &modern_graphics;
    if (g->deadline || !g->count) return;
    g->previous = current_settings();
    /* Set this first, so even a partial failure can restore the previous mode. */
    g->deadline = SDL_GetTicks() + 15000;
    if (!set_display(g->selected)) {
        SDL_Log("Modern graphics: cannot apply mode: %s", SDL_GetError());
        modern_graphics_cancel();
        SDL_strlcpy(g->status, "Ce mode est indisponible sur cet écran.", sizeof(g->status));
    } else {
        g->deadline = SDL_GetTicks() + 15000;
        g->status[0] = 0;
    }
}

void modern_graphics_confirm(void)
{
    ModernGraphics* g = &modern_graphics;
    if (!g->deadline) return;
    ModernDisplaySettings settings = current_settings();
    FILE* file = fopen("modern-ui/graphics.ini.tmp", "w");
    bool saved = false;
    if (file) {
        int written = fprintf(file, "Mode=%d\nWidth=%d\nHeight=%d\n", settings.mode, settings.width, settings.height);
        int closed = fclose(file);
        saved = written > 0 && closed == 0 && rename("modern-ui/graphics.ini.tmp", "modern-ui/graphics.ini") == 0;
    }
    g->deadline = 0;
    modern_graphics_begin();
    SDL_strlcpy(g->status, saved ? "Réglages enregistrés." :
        "Réglages appliqués, mais leur enregistrement a échoué.", sizeof(g->status));
}

void modern_graphics_tick(void)
{
    if (modern_graphics.deadline && SDL_GetTicks() >= modern_graphics.deadline) modern_graphics_cancel();
}

void modern_graphics_init(SDL_Window* window)
{
    display_window = window;
    ModernDisplaySettings settings;
    FILE* file = fopen("modern-ui/graphics.ini", "r");
    if (file) {
        int read = fscanf(file, "Mode=%d\nWidth=%d\nHeight=%d", &settings.mode, &settings.width, &settings.height);
        fclose(file);
        if (read == 3 && !SDL_getenv("ARCANUM_IGNORE_DISPLAY_SETTINGS")) {
            ModernDisplaySettings previous = current_settings();
            if (!set_display(settings)) set_display(previous);
        }
    }
    modern_graphics_begin();
}
