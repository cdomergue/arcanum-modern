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
#include "game/proto.h"
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




    int64_t scroll=0;assert(object_create(sub_4685A0(11059),obj_field_int64_get(pc,OBJ_F_LOCATION),&scroll));assert(scroll);
    assert(item_transfer(scroll,pc));
    obj_field_int32_set(scroll,OBJ_F_ITEM_FLAGS,obj_field_int32_get(scroll,OBJ_F_ITEM_FLAGS)|OIF_IDENTIFIED);
    obj_field_int32_set(scroll,OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY,10);
    for(int i=0;i<5;i++)obj_field_int32_set(scroll,OBJ_F_ITEM_SPELL_1+i,i?10000:42);
    ObjectID scroll_id=obj_get_id(scroll);int invloc=item_inventory_location_get(scroll);
    selector=1;selector_spells_collect(pc);bool found=false;
    for(int page=0;page<selector_spell_pages;page++){
        tactical_spell_page=page;selector_spells_collect(pc);
        for(int i=0;i<selector_spell_count;i++)if(selector_spells[i].item==scroll){found=true;break;}
        if(found)break;
    }
    assert(found);button_count=0;draw_tactical_spells();draw_binding_bar();
    int action=-1;for(int i=0;i<selector_spell_count;i++)if(selector_spells[i].item==scroll)action=1540+i;
    for(int i=0;i<button_count;i++)if(buttons[i].action==action){mouse_x=buttons[i].rect.x+20;mouse_y=buttons[i].rect.y+20;}
    QuickBinding binding=binding_hover();assert(binding.type==HOTKEY_ITEM_SPELL&&binding.item==scroll&&binding.data==42);
    SDL_Event key={0};key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_9;
    assert(handle_event(&key));binding_tick();key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key));
    assert(modern_hotkey_get(8)->type==HOTKEY_ITEM_SPELL&&modern_hotkey_get(8)->item_obj.obj==scroll);
    SDL_Event shift={0};shift.type=SDL_EVENT_KEY_DOWN;shift.key.scancode=SDL_SCANCODE_LSHIFT;shift.key.mod=SDL_KMOD_LSHIFT;handle_event(&shift);
    SDL_Event drag={0};drag.type=SDL_EVENT_MOUSE_BUTTON_DOWN;drag.button.button=SDL_BUTTON_LEFT;
    assert(binding_event(&drag));SDL_Delay(150);assert(binding_pressed);
    mouse_x=binding_slots[9].x+12;mouse_y=binding_slots[9].y+12;drag.type=SDL_EVENT_MOUSE_BUTTON_UP;assert(binding_event(&drag));
    shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);binding_tick();
    assert(modern_hotkey_get(19)->type==HOTKEY_ITEM_SPELL&&modern_hotkey_get(19)->data==42);
    assert(item_inventory_location_get(scroll)==invloc);
    for(int bank=0;bank<2;bank++){
        hotbar_mods=bank?SDL_KMOD_LSHIFT:0;button_count=0;
        SDL_SetRenderDrawColor(renderer,28,29,26,255);SDL_RenderClear(renderer);
        draw_tactical_spells();draw_binding_bar();
        char path[4096];snprintf(path,sizeof(path),"%s-%d.png",argv[3],bank);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
    selector=0;int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON);
    modern_hotkey_activate(8);assert(modern_spell_selected()==42);spell_ui_cancel();
    modern_hotkey_activate(19);assert(modern_spell_selected()==42);spell_ui_cancel();
    assert(item_wield_get(pc,ITEM_INV_LOC_WEAPON)==weapon);
    assert(gamelib_save("SlotScrollHotkeys","Scroll shortcuts"));
    assert(gamelib_load("SlotScrollHotkeys"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    scroll=obj_pool_perm_lookup(scroll_id);assert(scroll);
    assert(modern_hotkey_get(8)->item_obj.obj==scroll&&modern_hotkey_get(19)->item_obj.obj==scroll);
    assert(modern_hotkey_assign(8,HOTKEY_SKILL,0,0));assert(item_inventory_location_get(scroll)==invloc);
    assert(modern_hotkey_assign(8,HOTKEY_ITEM_SPELL,42,scroll));
    object_destroy(scroll);
    assert(gamelib_save("SlotScrollUsed","Used scroll shortcuts"));
    assert(gamelib_load("SlotScrollUsed"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    assert(modern_hotkey_get(8)->type==HOTKEY_NONE&&modern_hotkey_get(19)->type==HOTKEY_NONE);
    puts("PASS: scroll-list number and held drag assignment, both banks, item-based activation, save/load, replacement and removed scroll");
    return 0;
}
