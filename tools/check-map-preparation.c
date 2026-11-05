/* Run only against an isolated game copy marked .map-export-sandbox. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "ui/iso.h"
#define modern_maps_start test_maps_start
#define modern_maps_progress test_maps_progress
#define modern_maps_cached_ready test_maps_ready
#include "../source/src/ui/modern_ui.c"
static int starts, polls, choice=1;
static bool ready;
bool test_maps_ready(void){return ready;}
SDL_Process* test_maps_start(const char* executable)
{
    (void)executable;starts++;polls=0;
    const char* args[]={"/usr/bin/sleep","30",NULL};
    SDL_Process* p=SDL_CreateProcess(args,false);assert(p);return p;
}
void test_maps_progress(ModernMapProgress* progress)
{
    (void)progress;
    polls++;
    if(choice==2) {
        if(polls!=2 && polls!=3 && polls!=7)return;
        SDL_Event key={0};key.type=polls==7?SDL_EVENT_KEY_UP:SDL_EVENT_KEY_DOWN;
        key.key.scancode=SDL_SCANCODE_ESCAPE;key.key.repeat=polls==3;
        assert(SDL_PushEvent(&key));return;
    }
    if(polls!=2)return;
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
    assert(surface&&SDL_SavePNG(surface,starts==1?"/tmp/maps-startup.png":"/tmp/maps-preparation.png"));SDL_DestroySurface(surface);
    float x,y;SDL_RenderCoordinatesToWindow(renderer,canvas_w/2+(choice?100:-200),canvas_h/2+160,&x,&y);
    SDL_Event e={0};e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.button=SDL_BUTTON_LEFT;e.button.x=x;e.button.y=y;
    assert(SDL_PushEvent(&e));
}

void sub_549A70(void);
void sub_5412D0(void);

int main(int argc,char** argv)
{
    if(argc!=4 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1600;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());assert(gamelib_load(argv[2]));sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);in_game=true;
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1600,900);
    assert(target && SDL_SetRenderTarget(renderer,target));update_geometry();
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);


    skip_map_generation=false;save_settings();
    assert(modern_ui_prepare_maps("unused"));assert(starts==1 && skip_map_generation);
    skip_map_generation=false;load_settings();assert(skip_map_generation);
    assert(modern_ui_prepare_maps("unused"));assert(starts==1);
    choice=0;assert(prepare_maps("unused",true));assert(starts==2 && skip_map_generation);
    skip_map_generation=false;save_settings();
    assert(modern_ui_prepare_maps("unused"));assert(starts==3 && !skip_map_generation);
    load_settings();assert(!skip_map_generation);
    ready=true;assert(prepare_maps("unused",true));assert(starts==3);
    mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();
    for(int scale=100;scale<=150;scale+=50){
        text_scale=scale;button_count=0;draw_menu();
        bool found=false;for(int i=0;i<button_count;i++)if(buttons[i].action==446)found=true;
        assert(found);
        char path[128];snprintf(path,sizeof(path),"/tmp/maps-options-%d.png",scale);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
    ready=false;pending_action=446;modern_ui_menu_tick();assert(starts==4);
    choice=2;map_cache_id=42;assert(prepare_maps("unused",true));assert(starts==5 && polls==7 && map_cache_id==-1);
    puts("PASS: persistent opt-out, automatic bypass, temporary skip, manual override and options dispatch");
    return 0;
}
