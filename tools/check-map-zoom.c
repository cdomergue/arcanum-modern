/* Build with tools/build-map-zoom-check.py; run only on a marked game copy.
 * Usage: check-map-zoom GAME SAVE_SLOT OUTPUT_PREFIX */
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
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/wmap_ui.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);
void sub_5412D0(void);
/* Test fixture: exercise the travel view even when the save is in a town. */
void check_travel_map_open(void);
void check_travel_map_backend(void (*moving_frame)(void));

static void paint(const char* prefix,const char* name)
{
    button_count=panel_region_count=0;
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    draw_map();
    if(name){
        char path[4096];snprintf(path,sizeof(path),"%s-%s.png",prefix,name);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
        assert(surface && SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
}
static void wheel(float delta,bool flipped,float x,float y)
{
    int w,h;SDL_GetWindowSize(window,&w,&h);
    SDL_Event event={.type=SDL_EVENT_MOUSE_WHEEL};
    event.wheel.y=delta;event.wheel.direction=flipped?SDL_MOUSEWHEEL_FLIPPED:SDL_MOUSEWHEEL_NORMAL;
    event.wheel.mouse_x=x*w/canvas_w;event.wheel.mouse_y=y*h/canvas_h;
    assert(handle_ui_event(&event));
}
static void pointer(Uint32 type,float x,float y)
{
    int w,h;SDL_GetWindowSize(window,&w,&h);
    SDL_Event event={.type=type};
    if(type==SDL_EVENT_MOUSE_MOTION){event.motion.x=x*w/canvas_w;event.motion.y=y*h/canvas_h;}
    else {event.button.button=SDL_BUTTON_LEFT;event.button.x=x*w/canvas_w;event.button.y=y*h/canvas_h;}
    assert(panels_event(&event));
}
static void moving_frame(void)
{
    ModernTravelMap travel;assert(modern_map_travel_state(&travel));
    paint(NULL,NULL);
    assert(map_camera_travel && map_texture);
    assert(travel.x>=map_camera.x && travel.x<=map_camera.x+map_camera.width);
    assert(travel.y>=map_camera.y && travel.y<=map_camera.y+map_camera.height);
}
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
    wmap_ui_open();panels_tick();modern_map_action(2);paint(argv[3],"full");
    ModernMap native;assert(modern_map_state(&native)&&native.mode==1);
    assert(map_camera_active&&!map_camera_local);
    assert(map_continent_texture_width>native.width && map_continent_texture_height>native.height);
    float fit=map_camera.scale;
    float x=map_destination.x+map_destination.w*.5f,y=map_destination.y+map_destination.h*.5f;
    mouse_x=mouse_y=0; /* The wheel's own coordinates must be used. */
    wheel(3,false,x,y);paint(argv[3],"zoom");assert(map_camera.scale>fit);
    assert(modern_map_state(&native)&&native.mode==1);
    float zoom=map_camera.scale;
    wheel(1,false,0,0);assert(map_camera.scale==zoom);
    wheel(-1,true,x,y);assert(map_camera.scale>zoom);
    paint(argv[3],NULL);
    x=map_destination.x+map_destination.w*.5f;y=map_destination.y+map_destination.h*.5f;
    float before=map_camera.y;
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);SDL_Delay(40);
    pointer(SDL_EVENT_MOUSE_MOTION,x-40,y-40);panels_tick();paint(argv[3],NULL);
    assert(map_pressed&&map_dragged&&map_camera.y>before);
    zoom=map_camera.scale;wheel(-1,false,x,y);assert(map_camera.scale==zoom);
    SDL_Delay(40);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x-40,y-40);panels_tick();paint(argv[3],"pan");
    assert(!map_pressed&&modern_map_state(&native)&&native.mode==1);
    /* Personal notes must move with their native map coordinates. */
    notebook_sync();map_memo_count=1;memos_visible=true;
    map_memos[0]=(MapMemo){.mode=1,.map=0,.x=map_camera.x+map_camera.width/2,.y=map_camera.y+map_camera.height/2};
    strcpy(map_memos[0].text,"Zoom check");paint(argv[3],"note");
    bool found=false;for(int i=0;i<button_count;i++)if(buttons[i].action==2200)found=true;
    assert(found);
    for(int i=0;i<12;i++){wheel(-1,false,x,y);paint(argv[3],NULL);}
    assert(fabsf(map_camera.scale-fit)<.001f&&map_camera.x==0&&map_camera.y==0);
    assert(modern_map_state(&native)&&native.mode==1);paint(argv[3],"out");
    for(int i=0;i<12;i++){wheel(1,false,x,y);paint(argv[3],NULL);}
    assert(fabsf(map_camera.scale-4*fit)<.001f);
    assert(modern_map_state(&native)&&native.mode==1);
    for(int i=0;i<panel_region_count;i++)if(panel_regions[i].kind==3){
        assert(panel_regions[i].x==native.x+(int)map_camera.x);
        assert(panel_regions[i].y==native.y+(int)map_camera.y);
    }
    assert(panels_action(749));paint(argv[3],NULL);
    assert(map_camera.scale<4*fit);
    map_memo_count=0;
    /* Local maps still use their own world coordinates and zoom limits. */
    modern_map_action(1);paint(argv[3],NULL);assert(modern_map_state(&native)&&map_camera_local&&map_camera_id==native.townmap);
    float local=map_camera.scale;x=map_destination.x+map_destination.w/2;y=map_destination.y+map_destination.h/2;
    wheel(-1,false,x,y);paint(argv[3],NULL);assert(map_camera.scale<=local&&map_camera_local);
    wmap_ui_close();panels_tick();wmap_ui_open();panels_tick();modern_map_action(2);paint(argv[3],NULL);
    assert(map_camera.scale==fit&&!map_camera_local);
    check_travel_map_open();paint(argv[3],"travel-start");
    assert(map_camera_travel && !map_camera_local && map_texture);
    assert(modern_map_state(&native)&&native.mode==0);
    float travel_scale=map_camera.scale;
    float initial_width=map_camera.width;
    assert(map_label_count>0);float label_height=map_label_rects[0].h;
    assert(map_marker_count>0);float marker_size=map_marker_rects[0].w;
    x=map_destination.x+map_destination.w/2;y=map_destination.y+map_destination.h/2;
    for(int i=0;i<4;i++){wheel(-1,false,x,y);paint(argv[3],NULL);}
    assert(map_camera.scale<travel_scale && map_camera.width>initial_width);
    assert(modern_map_state(&native)&&native.mode==0);
    paint(argv[3],"travel-out");
    for(int i=0;i<20;i++){wheel(-1,false,x,y);paint(argv[3],NULL);}
    assert(map_camera.width==map_camera_width&&map_camera.height==map_camera_height);
    assert(map_camera.x==0&&map_camera.y==0);
    paint(argv[3],"travel-full");
    assert(map_label_count>0);
    for(int i=0;i<map_label_count;i++)assert(map_label_rects[i].h==label_height);
    int isolated=-1;
    for(int i=0;i<map_marker_count;i++) {
        assert(map_marker_rects[i].w==marker_size && map_marker_rects[i].h==marker_size);
        if(!map_markers[i].selectable)continue;
        bool clear=true;
        for(int j=0;j<map_marker_count;j++)if(j!=i && map_markers[j].selectable) {
            float dx=map_marker_rects[i].x-map_marker_rects[j].x,dy=map_marker_rects[i].y-map_marker_rects[j].y;
            if(dx*dx+dy*dy<40*40)clear=false;
        }
        if(clear)isolated=i;
    }
    assert(isolated>=0);
    SDL_FRect marker=map_marker_rects[isolated];
    float mx=marker.x+marker.w/2+10,my=marker.y+marker.h/2;
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,mx,my);
    assert(map_press_x==map_markers[isolated].x && map_press_y==map_markers[isolated].y);
    SDL_Delay(40);pointer(SDL_EVENT_MOUSE_MOTION,mx-20,my);
    pointer(SDL_EVENT_MOUSE_BUTTON_UP,mx-20,my);panels_tick();paint(argv[3],NULL);
    SDL_FRect label=map_label_rects[0];
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,label.x+label.w/2,label.y+label.h/2);
    assert(map_press_x==map_label_x[0] && map_press_y==map_label_y[0]);
    SDL_Delay(40);pointer(SDL_EVENT_MOUSE_MOTION,label.x+label.w/2-10,label.y+label.h/2);
    pointer(SDL_EVENT_MOUSE_BUTTON_UP,label.x+label.w/2-10,label.y+label.h/2);panels_tick();paint(argv[3],NULL);
    for(int i=0;i<6;i++){wheel(1,false,x,y);paint(argv[3],NULL);}
    float travel_y=map_camera.y;
    x=map_destination.x+map_destination.w/2;y=map_destination.y+map_destination.h/2;
    pointer(SDL_EVENT_MOUSE_BUTTON_DOWN,x,y);SDL_Delay(40);
    pointer(SDL_EVENT_MOUSE_MOTION,x-20,y-20);panels_tick();paint(argv[3],NULL);
    SDL_Delay(40);pointer(SDL_EVENT_MOUSE_BUTTON_UP,x-20,y-20);panels_tick();
    assert(map_camera.y>travel_y);
    paint(argv[3],"travel-pan");
    for(int i=0;i<panel_region_count;i++)if(panel_regions[i].kind==3){
        assert(panel_regions[i].x==(int)map_camera.x);
        assert(panel_regions[i].y==(int)map_camera.y);
    }
    assert(modern_map_state(&native)&&native.mode==0);
    wmap_ui_close();panels_tick();wmap_ui_open();panels_tick();check_travel_map_open();paint(argv[3],NULL);
    assert(map_camera_travel && map_camera.scale==travel_scale);
    check_travel_map_backend(moving_frame);
    assert(world_zoom==1);
    puts("PASS: travel and continent continuous zoom, full-map fit, pan with delayed release, native coordinates, notes, local map and reopening");
    return 0;
}
