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

static void render(const char* capture)
{
    button_count=0;SDL_SetRenderDrawColor(renderer,17,13,9,255);SDL_RenderClear(renderer);
    drawing_play_panel=true;draw_play_panels();drawing_play_panel=false;
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
    int before=auto_level_scheme_get(modern_charedit_object());
    assert(handle_event(&event));modern_ui_tick();SDL_Delay(150);modern_ui_tick();
    assert(auto_level_scheme_get(modern_charedit_object())==before);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
    assert(handle_event(&event));modern_ui_tick();
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=argc==4?atoi(argv[3]):1360;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    canvas_w=init.width;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,init.width,900);
    assert(target&&SDL_SetRenderTarget(renderer,target));SDL_SetRenderLogicalPresentation(renderer,init.width,900,SDL_LOGICAL_PRESENTATION_DISABLED);
    assert(gamelib_load("Slot0003"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj(),companions[8];
    object_flags_unset(pc,OF_OFF);
    assert(modern_followers_get(companions,8)>0);
    assert(charedit_open(companions[0],CHAREDIT_MODE_PASSIVE));panels_tick();
    render(argv[2]);
    int old_scheme=auto_level_scheme_get(companions[0]);
    int pc_scheme=auto_level_scheme_get(pc);
    int strength=stat_base_get(companions[0],STAT_STRENGTH);
    int points=stat_base_get(companions[0],STAT_UNSPENT_POINTS);
    click(654);
    assert(sheet_category==4 && modern_charedit_scheme_editable() && !modern_charedit_editable());
    int count=modern_charedit_scheme_count();assert(count>16);
    int original_index=-1;
    for(int i=0;i<count;i++)if(modern_charedit_scheme_id(i)==old_scheme)original_index=i;
    assert(original_index>=0 && sheet_scheme_page==original_index/8);
    char scheme_capture[1024];snprintf(scheme_capture,sizeof(scheme_capture),"%s-schemes.png",argv[2]);render(scheme_capture);
    while(sheet_scheme_page>0)click(655);
    click(660);assert(auto_level_scheme_get(companions[0])==0);
    while(sheet_scheme_page<(count-1)/8)click(656);
    click(660+(count-1)%8);
    int chosen=modern_charedit_scheme_id(count-1);
    assert(auto_level_scheme_get(companions[0])==chosen);
    assert(auto_level_scheme_get(pc)==pc_scheme);
    assert(stat_base_get(companions[0],STAT_STRENGTH)==strength);
    assert(stat_base_get(companions[0],STAT_UNSPENT_POINTS)==points);
    modern_charedit_scheme_select(-1);modern_charedit_scheme_select(count);
    assert(auto_level_scheme_get(companions[0])==chosen);
    char capture[1024];snprintf(capture,sizeof(capture),"%s-last.png",argv[2]);render(capture);
    click(651);assert(sheet_category==1);
    click(653);assert(sheet_category==3);
    click(654);assert(sheet_scheme_page==(count-1)/8);
    click(720);panels_tick();assert(!charedit_is_created());
    assert(gamelib_save("SlotSchm","Scheme test"));
    assert(gamelib_load("SlotSchm"));sub_5412D0();
    pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    assert(modern_followers_get(companions,8)>0);
    assert(auto_level_scheme_get(companions[0])==chosen && auto_level_scheme_get(pc)==pc_scheme);
    assert(charedit_open(companions[0],CHAREDIT_MODE_PASSIVE));panels_tick();click(654);
    while(sheet_scheme_page>original_index/8)click(655);
    while(sheet_scheme_page<original_index/8)click(656);
    click(660+original_index%8);assert(auto_level_scheme_get(companions[0])==old_scheme);
    click(720);panels_tick();
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));panels_tick();click(654);
    while(sheet_scheme_page>0)click(655);
    click(661);assert(auto_level_scheme_get(pc)==modern_charedit_scheme_id(1));
    click(720);panels_tick();
    assert(gamelib_delete("SlotSchmScheme test"));
    puts("PASS: companion and PC scheme changes, held-click release, all pages, none/custom schemes, tab re-entry, unchanged stats and save/load persistence");
    return 0;
}
