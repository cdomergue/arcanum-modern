#undef NDEBUG
#include "../source/first_party/tig/src/mouse.c"
#include <stdio.h>

int main(void)
{
    SDL_Surface* canvas = SDL_CreateSurface(320, 240, SDL_PIXELFORMAT_XRGB8888);
    SDL_Renderer* renderer = SDL_CreateSoftwareRenderer(canvas);
    assert(renderer);
    TigVideoBufferCreateInfo info = {0};
    info.width = 12; info.height = 16;
    info.flags = TIG_VIDEO_BUFFER_CREATE_COLOR_KEY;
    info.color_key = 0xff00ff;
    assert(tig_video_buffer_create(&info, &tig_mouse_cursor_video_buffer) == TIG_OK);
    assert(tig_video_buffer_fill(tig_mouse_cursor_video_buffer, NULL, 0xffffff) == TIG_OK);
    tig_mouse_active = true;
    tig_mouse_state = (TigMouseState){0};
    tig_mouse_state.frame.width = 12; tig_mouse_state.frame.height = 16;
    tig_mouse_state.offset_x = 3; tig_mouse_state.offset_y = 4;
    const int world_widths[] = {800, 1600, 3200};
    for (int i = 0; i < 3; i++) {
        tig_mouse_resize_screen(world_widths[i], world_widths[i] * 3 / 4);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
        tig_mouse_display_native(renderer, 100, 80, 1);
        SDL_RenderPresent(renderer);
        int count = 0;
        for (int y = 0; y < 240; y++) {
            Uint32* row = (Uint32*)((char*)canvas->pixels + y * canvas->pitch);
            for (int x = 0; x < 320; x++) if ((row[x] & 0xffffff) == 0xffffff) {
                assert(x >= 97 && x < 109 && y >= 76 && y < 92);
                count++;
            }
        }
        assert(count == 12 * 16);
    }
    tig_mouse_state.flags = TIG_MOUSE_STATE_HIDDEN;
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); SDL_RenderClear(renderer);
    tig_mouse_display_native(renderer, 100, 80, 1); SDL_RenderPresent(renderer);
    assert((((Uint32*)canvas->pixels)[80 * 320 + 100] & 0xffffff) == 0);
    tig_mouse_cursor_destroy_video_buffers();
    SDL_DestroyRenderer(renderer); SDL_DestroySurface(canvas);
    puts("PASS: cursor pixel size and hotspot stay fixed across world resolutions; hidden cursor stays hidden");
    return 0;
}
