/* Link in place of main.c and modern_ui.c. Use only an isolated marked copy. */
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
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);

static void action(int id)
{
    pending_action=id;modern_ui_menu_tick();
}
static void render(const char* capture)
{
    button_count=0;SDL_SetRenderDrawColor(renderer,17,13,9,255);SDL_RenderClear(renderer);
    draw_menu();
    if(capture){SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,capture));SDL_DestroySurface(surface);}
}
static void click(int id)
{
    render(NULL);Button* target=NULL;
    for(int i=0;i<button_count;i++)if(buttons[i].action==id)target=&buttons[i];
    assert(target&&target->enabled);
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_Event event={0};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
    event.button.x=(target->rect.x+target->rect.w/2)*ww/canvas_w;
    event.button.y=(target->rect.y+target->rect.h/2)*wh/canvas_h;
    int old=settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY);
    assert(handle_event(&event));modern_ui_menu_tick();SDL_Delay(150);modern_ui_menu_tick();
    assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==old);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
    assert(handle_event(&event));modern_ui_menu_tick();
}
int main(int argc,char** argv)
{
    if(argc!=3||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
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
    canvas_w=1360;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1360,900);
    assert(target&&SDL_SetRenderTarget(renderer,target));SDL_SetRenderLogicalPresentation(renderer,1360,900,SDL_LOGICAL_PRESENTATION_DISABLED);
    mainmenu_ui_window_type_set(MM_WINDOW_MAINMENU);mainmenu_ui_open();modern_ui_menu_tick();
    mainmenu_ui_close(false);mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(406);
    assert(options_tab==0&&settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==1);
    click(419);assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==0);
    render(argv[2]);
    Settings saved;settings_init(&saved,"arcanum.cfg");settings_load(&saved);
    settings_register(&saved,MODERN_FOLLOWER_AUTO_PICKUP_KEY,"1",NULL);
    assert(settings_get_value(&saved,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==0);settings_exit(&saved);
    action(408);assert(options_tab==1);action(419);assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==0);
    action(406);click(419);assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==1);
    click(419);assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==0);
    action(405);assert(mainmenu_ui_window_type_get()!=MM_WINDOW_OPTIONS);
    mainmenu_ui_close(false);mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(406);
    assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==0);render(NULL);
    puts("PASS: live main/options transitions, held-click release, persisted toggle, other-tab guard and menu re-entry");
    return 0;
}
