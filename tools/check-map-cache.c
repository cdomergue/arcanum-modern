#include <stdio.h>
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
#include "../source/src/ui/modern_ui.c"

static bool settle_map(const ModernMap* map,SDL_FRect rect,float scale)
{
    for(int i=0;i<10000;i++) {
        if(map){if(!map_cache_draw(map,rect,scale))return false;}else draw_map();
        if(!map_cache_available)return false;
        SDL_LockMutex(map_loader.mutex);bool busy=false;
        for(int j=0;j<MAP_CACHE_JOBS;j++)if(map_loader.jobs[j].state)busy=true;
        SDL_UnlockMutex(map_loader.mutex);
        if(!busy)return true;
        SDL_Delay(1);
    }
    return false;
}

int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc!=4 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};
    init.width=1728;init.height=1084;init.bpp=32;
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
    wmap_ui_open();
    ModernMap map;
    if(!modern_map_state(&map))return 7;
    if(!map.townmap){modern_map_action(2);if(!modern_map_state(&map) || !map.townmap)return 8;}
    printf("PREVIEW townmap=%d offsets=%d,%d origin=%d,%d size=%d,%d\n",
        map.townmap,map.offset_x,map.offset_y,map.origin_x,map.origin_y,map.width,map.height);
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1728,1084);
    if(!target || !SDL_SetRenderTarget(renderer,target))return 10;
    update_geometry();
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    if(!settle_map(NULL,(SDL_FRect){0},0))return 20;
    if(!map_texture_hd || !map_cache_available)return 11;
    SDL_Surface* image=SDL_RenderReadPixels(renderer,NULL);
    if(!image || !SDL_SavePNG(image,argv[3]))return 9;
    SDL_DestroySurface(image);
    map_bitmap_clear();
    map_cache_available=false;
    draw_map();
    int fallback_tiles=0;
    for(int i=0;i<map_bitmap_count;i++)if(map_bitmap_tiles[i])fallback_tiles++;
    if(map_cache_available || !fallback_tiles)return 12;
    char fallback[4096];snprintf(fallback,sizeof(fallback),"%s.original.png",argv[3]);
    image=SDL_RenderReadPixels(renderer,NULL);
    if(!image || !SDL_SavePNG(image,fallback))return 13;
    SDL_DestroySurface(image);
    ModernMap fogmap={.townmap=33,.width=512,.height=512,.offset_x=100,.offset_y=100};
    if(!map_cache_prepare(&fogmap) || townmap_is_waitable(33))return 14;
    townmap_set_known(33,false);
    SDL_SetRenderLogicalPresentation(renderer,1728,1084,SDL_LOGICAL_PRESENTATION_DISABLED);
    SDL_SetRenderDrawColor(renderer,255,0,255,255);SDL_RenderClear(renderer);
    if(!settle_map(&fogmap,(SDL_FRect){0,0,512,512},1))return 15;
    image=SDL_RenderReadPixels(renderer,NULL);
    if(!image)return 16;
    for(int y=2;y<510;y++)for(int x=2;x<510;x++) {
        Uint8 r,g,b,a;
        if(!SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a) || r || g || b)return 17;
    }
    SDL_DestroySurface(image);
    townmap_set_known(33,true);
    if(!settle_map(&fogmap,(SDL_FRect){0,0,512,512},1))return 18;
    image=SDL_RenderReadPixels(renderer,NULL);
    int visible_pixels=0;
    for(int y=2;y<510;y++)for(int x=2;x<510;x++) {
        Uint8 r,g,b,a;SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a);
        if(r || g || b)visible_pixels++;
    }
    SDL_DestroySurface(image);
    if(!visible_pixels)return 19;
    /* The cached composition must hide explored pixels when fog changes. */
    townmap_set_known(33,false);
    if(!map_cache_draw(&fogmap,(SDL_FRect){0,0,512,512},1))return 21;
    image=SDL_RenderReadPixels(renderer,NULL);
    for(int y=2;y<510;y++)for(int x=2;x<510;x++) {
        Uint8 r,g,b,a;SDL_ReadSurfacePixel(image,x,y,&r,&g,&b,&a);
        if(r||g||b)return 22;
    }
    SDL_DestroySurface(image);map_cache_clear();
    const int maps[]={3,21,44,45,46,47};
    for(size_t i=0;i<SDL_arraysize(maps);i++) {
        ModernMap area={.townmap=maps[i]};
        if(!map_cache_prepare(&area))return 23;
        townmap_set_known(maps[i],true);
        float scale=fminf(1600.0f/map_cache_info.width,900.0f/map_cache_info.height)/map_cache_info.scale;
        area.width=(int)ceilf(1600/scale);area.height=(int)ceilf(900/scale);
        if(!settle_map(&area,(SDL_FRect){0,0,1600,900},scale))return 24;
        Uint64 version=map_cache_view_revision,start=SDL_GetTicksNS();
        for(int frame=0;frame<30;frame++) {
            if(!map_cache_draw(&area,(SDL_FRect){0,0,1600,900},scale))return 25;
            SDL_FlushRenderer(renderer);
            if(map_cache_view_revision!=version)return 26;
        }
        printf("PASS map %d: steady composed view %.2f ms/frame, %zu resident bytes\n",maps[i],(SDL_GetTicksNS()-start)/30000000.0,map_cache_bytes);
        image=SDL_RenderReadPixels(renderer,NULL);
        char shot[4096];snprintf(shot,sizeof(shot),"%s-map%d.png",argv[3],maps[i]);
        if(!image||!SDL_SavePNG(image,shot))return 27;
        SDL_DestroySurface(image);
    }
    map_cache_clear();
    printf("PASS native map, original fallback, unknown fog, explored visibility (%d pixels)\n",visible_pixels);
    return 0;
}
