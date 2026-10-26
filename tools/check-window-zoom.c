/* Link in place of main.c and modern_ui.c; requires an isolated marked copy.
 * Usage: check-window-zoom GAME CAPTURE_DIRECTORY [SAVE_SLOT] */
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
#include "ui/sleep_ui.h"
#include "ui/fate_ui.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);


static const char* capture_dir;
static int expected_width,expected_height;
static void check_scale(void)
{
    assert(hrp_iso_window_width_get()==expected_width);
    assert(hrp_iso_window_height_get()==expected_height);
}
static void capture_frame(const char* name)
{
    check_scale();iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();check_scale();
    if(name){
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface);
        char path[4096];snprintf(path,sizeof(path),"%s/%s.png",capture_dir,name);
        assert(SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
}
static void opened(int panel,const char* name)
{
    printf("Open %s: panel %d, zoom %.2f\n",name?name:"window",native_panel(),world_zoom);
    assert(native_panel()==panel);check_scale();modern_ui_tick();check_scale();
    SDL_Event event;SDL_zero(event);event.type=SDL_EVENT_MOUSE_WHEEL;
    event.wheel.y=-1;event.wheel.mouse_x=680;event.wheel.mouse_y=450;
    float saved=world_zoom;assert(!world_zoom_event(&event)&&world_zoom==saved);
    capture_frame(name);
}
static void closed(int action,const char* name)
{
    if(action>=0){pending_action=action;modern_ui_tick();}
    assert(!native_panel()&&!journal&&!mainmenu_ui_is_active());
    capture_frame(name); /* First exposed frame, before another tick can repair it. */
    int64_t x,y,xx,yy;location_origin_get(&x,&y);
    for(int i=0;i<3;i++){
        modern_ui_tick();capture_frame(NULL);location_origin_get(&xx,&yy);
        assert(x==xx&&y==yy);SDL_Delay(30);
    }
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1360;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);

    capture_dir=argv[2];
    assert(gamelib_load(argc==4?argv[3]:"Slot0002"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();


    const float zooms[]={.75f,1.25f};
    for(int i=0;i<2;i++){
        world_zoom=zooms[i];modern_ui_tick();
        expected_width=(int)lroundf(1360/world_zoom);expected_height=(int)lroundf(900/world_zoom);
        capture_frame(i?"zoom-in-before":"zoom-out-before");
        assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));opened(1,i?NULL:"sheet");closed(720,i?NULL:"sheet-closed");
        assert(inven_ui_open(pc,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));opened(2,i?NULL:"inventory");closed(730,i?NULL:"inventory-closed");
        wmap_ui_open();opened(3,i?NULL:"map");closed(740,i?NULL:"map-closed");
        logbook_ui_open(pc);assert(journal);opened(0,i?NULL:"journal");logbook_ui_close();closed(-1,i?NULL:"journal-closed");
        schematic_ui_toggle(pc,pc);opened(7,i?NULL:"schematics");closed(929,i?NULL:"schematics-closed");
        sleep_ui_toggle(OBJ_HANDLE_NULL);opened(6,i?NULL:"sleep");closed(908,i?NULL:"sleep-closed");
        fate_ui_toggle(pc);opened(8,i?NULL:"fate");closed(969,i?NULL:"fate-closed");
        mainmenu_ui_start(MM_TYPE_IN_PLAY);assert(mainmenu_ui_is_active());opened(0,i?NULL:"pause");
        closed(500+sub_5496C0(mainmenu_ui_window_type_get())->num_buttons-1,i?NULL:"pause-closed");
        mainmenu_ui_start(MM_TYPE_IN_PLAY);opened(0,NULL);
        sub_5412D0();closed(-1,NULL); /* Native close between UI ticks. */
        capture_frame(i?"zoom-in-after":"zoom-out-after");
    }
    puts("PASS: stable zoom and first exposed frame through sheet, inventory, map, journal, schematics, sleep, fate and pause at 75% and 125%");
    return 0;
}
