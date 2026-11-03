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



    assert(modern_hotkey_assign(0,HOTKEY_SKILL,0,0));
    assert(modern_hotkey_assign(10,HOTKEY_SKILL,3,0));
    int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON);assert(weapon);
    assert(modern_hotkey_assign(19,HOTKEY_ITEM,0,weapon));
    assert(modern_hotkey_get(0)->data==0&&modern_hotkey_get(10)->data==3);
    assert(gamelib_save("SlotHotbar","Two hotbars"));
    assert(gamelib_load("SlotHotbar"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    pc=player_get_local_pc_obj();assert(modern_hotkey_get(0)->data==0&&modern_hotkey_get(10)->data==3);
    assert(modern_hotkey_get(19)->type==HOTKEY_ITEM);
    modern_ui_tick();hud=true;
    SDL_Event shift={0};shift.type=SDL_EVENT_KEY_DOWN;shift.key.scancode=SDL_SCANCODE_LSHIFT;shift.key.mod=SDL_KMOD_LSHIFT;
    handle_event(&shift);SDL_Delay(180);assert(hotbar_offset()==10);
    SDL_Event digit={0};digit.type=SDL_EVENT_KEY_DOWN;digit.key.scancode=SDL_SCANCODE_1;digit.key.mod=SDL_KMOD_LSHIFT;
    assert(handle_event(&digit));assert(pending_action==310);
    /* Releasing Shift before dispatch must not select bank one. */
    shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);assert(hotbar_offset()==0&&pending_action==310);
    digit.key.repeat=true;assert(handle_event(&digit));assert(pending_action==310);
    digit.type=SDL_EVENT_KEY_UP;digit.key.mod=0;assert(handle_event(&digit));
    modern_ui_tick();assert(intgame_mode_get()==INTGAME_MODE_SKILL);
    intgame_mode_set(INTGAME_MODE_MAIN);
    for(int combat=0;combat<2;combat++) {
        if(combat){combat_critter_activate_combat_mode(pc);combat_turn_based_whos_turn_set(pc);for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}}
        for(int bank=0;bank<2;bank++){
            shift.type=bank?SDL_EVENT_KEY_DOWN:SDL_EVENT_KEY_UP;shift.key.mod=bank?SDL_KMOD_LSHIFT:0;handle_event(&shift);
            SDL_SetRenderDrawColor(renderer,28,29,26,255);SDL_RenderClear(renderer);button_count=0;
            drawing_hud=true;if(combat)draw_tactical_hud();else draw_hud();drawing_hud=false;draw_party();
            bool found=false;for(int i=0;i<button_count;i++)if(buttons[i].action==300+bank*10)found=true;assert(found);
            char path[4096];snprintf(path,sizeof(path),"%s-%d-%d.png",argv[3],combat,bank);
            SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
        }
    }

    selector=2;button_count=0;draw_selector();draw_binding_bar();
    assert(native_panel()==5&&binding_bar_visible);
    bool found_skill=false;
    for(int i=0;i<button_count;i++)if(buttons[i].action==881){mouse_x=buttons[i].rect.x+12;mouse_y=buttons[i].rect.y+12;found_skill=true;}
    assert(found_skill);
    digit.type=SDL_EVENT_KEY_DOWN;digit.key.repeat=false;digit.key.scancode=SDL_SCANCODE_2;digit.key.mod=SDL_KMOD_LSHIFT;
    assert(handle_event(&digit));assert(binding_pending.slot==11);
    shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);
    binding_tick();assert(modern_hotkey_get(11)->data==1);
    digit.type=SDL_EVENT_KEY_UP;digit.key.mod=0;handle_event(&digit);
    /* Drop while Shift is held, then release it before the deferred assignment. */
    shift.type=SDL_EVENT_KEY_DOWN;shift.key.mod=SDL_KMOD_LSHIFT;handle_event(&shift);
    SDL_Event drag={0};drag.type=SDL_EVENT_MOUSE_BUTTON_DOWN;drag.button.button=SDL_BUTTON_LEFT;
    assert(binding_event(&drag));assert(binding_pressed);SDL_Delay(150);
    mouse_x=binding_slots[2].x+10;mouse_y=binding_slots[2].y+10;
    drag.type=SDL_EVENT_MOUSE_BUTTON_UP;assert(binding_event(&drag));assert(binding_pending.slot==12);
    shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);
    binding_tick();assert(modern_hotkey_get(12)->data==1);
    shift.type=SDL_EVENT_KEY_DOWN;shift.key.mod=SDL_KMOD_LSHIFT;handle_event(&shift);
    button_count=0;SDL_SetRenderDrawColor(renderer,28,29,26,255);SDL_RenderClear(renderer);
    draw_selector();draw_binding_bar();
    SDL_Surface* binding_surface=SDL_RenderReadPixels(renderer,NULL);assert(binding_surface&&SDL_SavePNG(binding_surface,"/tmp/shift-hotbar-binding.png"));SDL_DestroySurface(binding_surface);
    selector=0;
    /* Two held Shift keys and focus loss must not leave the second bank stuck. */
    shift.type=SDL_EVENT_KEY_DOWN;shift.key.scancode=SDL_SCANCODE_RSHIFT;shift.key.mod=SDL_KMOD_SHIFT;handle_event(&shift);
    shift.type=SDL_EVENT_KEY_UP;shift.key.scancode=SDL_SCANCODE_LSHIFT;shift.key.mod=SDL_KMOD_RSHIFT;handle_event(&shift);assert(hotbar_offset()==10);
    shift.key.scancode=SDL_SCANCODE_RSHIFT;shift.key.mod=0;handle_event(&shift);assert(hotbar_offset()==0);
    shift.type=SDL_EVENT_KEY_DOWN;shift.key.mod=SDL_KMOD_RSHIFT;handle_event(&shift);assert(hotbar_offset()==10);
    SDL_Event focus={0};focus.type=SDL_EVENT_WINDOW_FOCUS_LOST;handle_event(&focus);assert(hotbar_offset()==0);
    int64_t allies[8];assert(modern_followers_get(allies,8)>0);
    combat_turn_based_whos_turn_set(allies[0]);sub_424070(allies[0],3,0,1);
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    assert(modern_hotkey_assign(10,HOTKEY_SKILL,2,0));assert(modern_hotkey_get(10)->data==2);
    combat_turn_based_whos_turn_set(pc);assert(modern_hotkey_get(10)->data==3);
    puts("PASS: second-bank save/load, item reference, held Shift/digit, delayed release, two Shift keys, focus loss and companion independence");
    return 0;
}
