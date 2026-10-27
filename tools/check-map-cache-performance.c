#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/location.h"
#include "game/sector.h"
#include "game/townmap.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include <stdatomic.h>
static _Atomic int png_loads,main_png_loads;
static SDL_ThreadID benchmark_thread;
static SDL_Surface* counted_png(const char* path){png_loads++;if(SDL_GetCurrentThreadID()==benchmark_thread)main_png_loads++;return SDL_LoadPNG(path);}
#define SDL_LoadPNG counted_png
#include "../source/src/ui/modern_ui.c"
#undef SDL_LoadPNG

int main(int argc,char** argv)
{
    if(argc!=4 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    benchmark_thread=SDL_GetCurrentThreadID();
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0",SDL_HINT_OVERRIDE);
    const char* driver=SDL_getenv("ARCANUM_MAP_TEST_RENDERER");
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,driver?"x11":"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,driver?driver:"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};
    init.width=2560;init.height=1440;init.bpp=32;
    init.texture_width=init.texture_height=2048;
    init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;
    init.sound_file_path_resolver=gsound_resolve_path;
    if(tig_init(&init)!=TIG_OK)return 3;
    intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};
    if(!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game))return 4;
    if(!gameuilib_init(&game) || !gamelib_mod_load("Arcanum") || !gameuilib_mod_load())return 5;
    gamelib_splash_clear();
    if(!modern_ui_init() || !gamelib_load(argv[2]))return 6;
    intgame_mode_set(INTGAME_MODE_MAIN);

    main_png_loads=0;
    SDL_SetRenderVSync(renderer,0);
    SDL_SetWindowTitle(window,"Arcanum map benchmark - isolated test");
    printf("Renderer: %s\n",SDL_GetRendererName(renderer));
    ModernMap map={.townmap=21};assert(map_cache_prepare(&map));
    SDL_SetRenderLogicalPresentation(renderer,2560,1440,SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderScale(renderer,1,1);
    printf("HD benchmark map %d, %dx%d, native scale %f\n",map.townmap,map_cache_info.width,map_cache_info.height,map_cache_info.scale);
    float factors[]={.25f,.125f,.0625f,.03125f,.0625f,.125f,.03125f};
    for(unsigned i=0;i<sizeof(factors)/sizeof(*factors);i++){
        float scale=factors[i]/map_cache_info.scale;
        map.width=(int)(1600/scale);map.height=(int)(900/scale);
        for(int frame=0;frame<3;frame++){
            png_loads=0;Uint64 started=SDL_GetTicksNS();
            assert(map_cache_draw(&map,(SDL_FRect){0,0,1600,900},scale));
            SDL_FlushRenderer(renderer);
            printf("factor %.5f frame %d: %.2f ms, %d PNG loads\n",factors[i],frame,(SDL_GetTicksNS()-started)/1000000.0,png_loads);
        }
    }

    wmap_ui_open();in_game=true;update_geometry();
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    draw_map();SDL_FlushRenderer(renderer);
    main_png_loads=0;
    printf("Full UI map %d, canvas %.0fx%.0f, scale %.5f minimum %.5f\n",map_cache_id,canvas_w,canvas_h,map_camera.scale,map_camera.min_scale);
    map_camera.scale=map_camera.min_scale;
#ifndef MAP_CACHE_BASELINE
    for(int settle=0;settle<10000;settle++){
        button_count=panel_region_count=0;draw_map();SDL_FlushRenderer(renderer);
        SDL_LockMutex(map_loader.mutex);bool busy=false;
        for(int i=0;i<MAP_CACHE_JOBS;i++)if(map_loader.jobs[i].state)busy=true;
        SDL_UnlockMutex(map_loader.mutex);
        if(!busy)break;
        SDL_Delay(1);
    }
#endif
    for(int frame=0;frame<40;frame++){
        png_loads=0;Uint64 started=SDL_GetTicksNS();button_count=panel_region_count=0;
        draw_map();SDL_FlushRenderer(renderer);
        SDL_RenderPresent(renderer);
        printf("Full UI frame %d: %.2f ms, %d PNG loads\n",frame,(SDL_GetTicksNS()-started)/1000000.0,png_loads);

    }
#ifndef MAP_CACHE_BASELINE
    assert(main_png_loads==0);printf("PASS: no PNG decoding on UI thread, resident bytes %zu\n",map_cache_bytes);
#endif
    draw_map();SDL_FlushRenderer(renderer);
    SDL_Surface* capture=SDL_RenderReadPixels(renderer,NULL);
    assert(capture&&SDL_SavePNG(capture,argv[3]));SDL_DestroySurface(capture);
    map_cache_clear();
    return 0;
}
