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
#include "game/sector.h"
#include "ui/gameuilib.h"
#include "ui/iso.h"
#include "ui/sleep_ui.h"
#include "ui/fate_ui.h"
#define modern_maintained_spells fixture_maintained
#define modern_spell_stop fixture_stop
#include "../source/src/ui/modern_ui.c"
#undef modern_maintained_spells
#undef modern_spell_stop
static int fixture_count,stopped_id=-1;
int fixture_maintained(int64_t actor,int* ids,int* spells,int capacity) {
    for(int i=0;i<fixture_count && i<capacity;i++){ids[i]=100+i;spells[i]=i*5;}
    return fixture_count<capacity?fixture_count:capacity;
}
bool fixture_stop(int64_t actor,int id){stopped_id=id;return true;}

void sub_549A70(void);
void sub_5412D0(void);

static void render_check(const char* path)
{
    modern_ui_tick();iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();
    if(path){SDL_Surface* image=SDL_RenderReadPixels(renderer,NULL);assert(image&&SDL_SavePNG(image,path));SDL_DestroySurface(image);}
}
static SDL_FRect action_rect(int action)
{
    for(int i=0;i<button_count;i++)if(buttons[i].action==action && buttons[i].enabled)return buttons[i].rect;
    fprintf(stderr,"Missing available action %d (panel %d, page %d)\n",action,native_panel(),maintained_page);assert(false);return (SDL_FRect){0};
}
static void held_click(int action)
{
    render_check(NULL);SDL_FRect r=action_rect(action);int w,h;SDL_GetWindowSize(window,&w,&h);
    SDL_Event event={0};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.down=true;event.button.button=SDL_BUTTON_LEFT;
    event.button.x=(r.x+r.w/2)*w/canvas_w;event.button.y=(r.y+r.h/2)*h/canvas_h;
    assert(handle_event(&event));SDL_Delay(140);render_check(NULL);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;assert(handle_event(&event));assert(pending_action==action);modern_ui_tick();
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
    modern_ui_tick();hud=true;assert(top_bar_texture);SDL_WarpMouseInWindow(window,0,0);
    char path[4096];snprintf(path,sizeof(path),"%s-full.png",argv[3]);render_check(path);
    SDL_FRect bar=top_bar_rect(0,0,TOP_BAR_SOURCE_WIDTH,TOP_BAR_SOURCE_HEIGHT);
    SDL_Rect crop={(int)bar.x-4,0,(int)bar.w+8,(int)(bar.y+bar.h+5)};
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,&crop);snprintf(path,sizeof(path),"%s-bar.png",argv[3]);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    /* Native screens must still open after a physical mouse hold/release. */
    held_click(1);assert(play_sheet());render_check(NULL);charedit_close();
    held_click(0);assert(journal);render_check(NULL);logbook_ui_close();
    held_click(3);assert(wmap_ui_is_created());render_check(NULL);wmap_ui_close();
    held_click(2);assert(inven_ui_is_created());render_check(NULL);inven_ui_destroy();
    held_click(9);
    int town=townmap_get(sector_id_from_loc(obj_field_int64_get(pc,OBJ_F_LOCATION)));
    assert(modern_sleep_count() || (town!=TOWNMAP_NONE && !townmap_is_waitable(town)));
    render_check(NULL);sleep_ui_close();
    held_click(4);assert(modern_fate_count());render_check(NULL);fate_ui_close();
    /* Controlled spell IDs exercise the real UI pagination and stop routing. */
    fixture_count=5;maintained_page=0;snprintf(path,sizeof(path),"%s-spells-1.png",argv[3]);render_check(path);
    held_click(2506);assert(maintained_page==1);snprintf(path,sizeof(path),"%s-spells-2.png",argv[3]);render_check(path);
    held_click(2500);assert(stopped_id==103);
    held_click(2505);assert(maintained_page==0);held_click(2502);assert(stopped_id==102);
    fixture_count=0;render_check(NULL);
    /* The renderer accepts real native times without changing the saved game. */
    DateTime now=sub_45A7C0();
    for(int hour=0;hour<=12;hour+=12) {
        render_check(NULL);DateTime sample=now;
        datetime_sub_milliseconds(&sample,datetime_get_hour(&sample)*3600000+datetime_get_minute(&sample)*60000);
        datetime_add_milliseconds(&sample,hour*3600000);
        top_bar_clock(top_bar_rect(1076,17,250,57),sample);
        surface=SDL_RenderReadPixels(renderer,&crop);snprintf(path,sizeof(path),"%s-%02d.png",argv[3],hour);
        assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
    /* Tooltips and geometry at a narrower game window, including the footer. */
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_FRect clock=top_bar_rect(1076,17,250,57);
    SDL_WarpMouseInWindow(window,(clock.x+clock.w/2)*ww/canvas_w,(clock.y+clock.h/2)*wh/canvas_h);
    snprintf(path,sizeof(path),"%s-clock-hover.png",argv[3]);render_check(path);
    SDL_SetWindowSize(window,1024,768);SDL_WarpMouseInWindow(window,0,0);update_geometry();
    fixture_count=5;maintained_page=1;
    snprintf(path,sizeof(path),"%s-1024.png",argv[3]);render_check(path);
    SDL_FRect first=action_rect(1),last=action_rect(9);
    assert(first.x>=0 && last.x+last.w<=canvas_w);
    held_click(2505);assert(maintained_page==0);
    puts("PASS: native panel transitions, held mouse releases, maintained spell pages and IDs, native clock rendering");
    return 0;
}
