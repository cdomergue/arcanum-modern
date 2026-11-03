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
#include "../source/src/ui/modern_ui.c"
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


    int64_t allies[8];assert(modern_followers_get(allies,8)>=2);
    combat_critter_activate_combat_mode(pc);assert(combat_turn_based_is_active());
    combat_turn_based_whos_turn_set(pc);
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    int initial=combat_action_points_get();assert(!combat_companion_ai_get());
    for(int mode=0;mode<2;mode++){
        if(mode){
            /* A held mouse press changes mode on release, not on press. */
            float x=(canvas_w-980)/2+837,y=canvas_h-126+94;
            SDL_Event event={0};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.down=true;
            int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
            event.button.button=SDL_BUTTON_LEFT;event.button.x=x*ww/canvas_w;event.button.y=y*wh/canvas_h;
            assert(handle_event(&event));SDL_Delay(150);assert(!combat_companion_ai_get());
            event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;assert(handle_event(&event));assert(pending_action==1109);
            modern_ui_tick();assert(combat_companion_ai_get());modern_ui_tick();assert(!combat_companion_ai_pending());
        }
        assert(combat_input_actor()==pc&&combat_action_points_get()==initial);
        assert(combat_player_controls(pc));assert(combat_player_controls(allies[0])==(mode==0));
        SDL_SetRenderDrawColor(renderer,28,29,26,255);SDL_RenderClear(renderer);button_count=0;
        drawing_hud=true;draw_tactical_hud();drawing_hud=false;draw_party();
        bool button_found=false;for(int i=0;i<button_count;i++)if(buttons[i].action==1109){assert(buttons[i].enabled);button_found=true;}
        assert(button_found);
        char path[4096];snprintf(path,sizeof(path),"%s-%d.png",argv[3],mode);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
    /* Ending combat and saving outside combat must preserve the last choice. */
    combat_reset();combat_critter_deactivate_combat_mode(pc);assert(!combat_turn_based_is_active()&&combat_companion_ai_get());
    assert(gamelib_save("SlotAiBetween","Companion preference between combats"));
    assert(gamelib_load("SlotAiBetween"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    assert(combat_companion_ai_get()&&!combat_companion_ai_pending());
    pc=player_get_local_pc_obj();assert(modern_followers_get(allies,8)>=2);
    combat_critter_activate_combat_mode(pc);assert(combat_turn_based_is_active());
    combat_turn_based_whos_turn_set(pc);
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    initial=combat_action_points_get();
    assert(combat_companion_ai_get()&&!combat_player_controls(allies[0])&&combat_player_controls(pc));
    /* Save/load a live combat with AI enabled, in the isolated copy only. */
    assert(gamelib_save("SlotAiTest","Companion AI test"));
    assert(gamelib_load("SlotAiTest"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    assert(combat_companion_ai_get());modern_ui_tick();assert(!combat_companion_ai_pending());
    pc=player_get_local_pc_obj();assert(combat_input_actor()==pc&&combat_action_points_get()==initial);
    assert(modern_followers_get(allies,8)>=2);
    combat_companion_ai_set(false);combat_companion_ai_sync();assert(combat_player_controls(allies[0]));
    combat_turn_based_whos_turn_set(allies[0]);
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    int before=combat_action_points_get();
    assert(combat_consume_action_points(allies[0],2));before-=2;
    combat_companion_ai_set(true);combat_companion_ai_sync();assert(combat_companion_ai_get());
    assert(combat_action_points_get()<=before);
    combat_companion_ai_set(false);
    for(int i=0;i<300 && combat_companion_ai_pending();i++){
        modern_ui_tick();tig_ping();gamelib_ping();SDL_Delay(20);
    }
    assert(!combat_companion_ai_get()&&!combat_companion_ai_pending());
    assert(combat_player_controls(allies[0]));
    if(combat_turn_based_whos_turn_get()==allies[0])assert(combat_action_points_get()<=before);
    puts("PASS: live AI toggle, held click, PC ownership, save/load, companion handoff and AP preservation");
    return 0;
}
