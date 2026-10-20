#include <stdio.h>
#undef NDEBUG
#include <assert.h>
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
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include "ui/follower_ui.h"
#include "game/combat.h"
#include "game/anim.h"
#include "ui/hotkey_ui.h"
#include "game/item.h"
#include "game/map.h"
#include "ui/skill_ui.h"
#include "../source/src/ui/modern_ui.c"

int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};
    init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;
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
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(!sub_45B300());

    int64_t pc=player_get_local_pc_obj(),allies[8];
    object_flags_unset(pc,OF_OFF);
    int count=modern_followers_get(allies,8);
    assert(count>=3);

    combat_critter_activate_combat_mode(pc);
    combat_turn_based_whos_turn_set(pc);
    for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
    assert(modern_hotkey_assign(9,HOTKEY_SKILL,0,0));
    Hotkey hero=*sub_57F240(9);
    combat_turn_based_whos_turn_set(allies[0]);
    sub_424070(allies[0],3,0,1);
    for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
    assert(modern_hotkey_assign(9,HOTKEY_SKILL,3,0));
    assert(sub_57F240(9)->type==hero.type && sub_57F240(9)->data==hero.data);
    ObjectID owners[8];
    for(int i=0;i<count;i++)owners[i]=obj_get_id(allies[i]);
    assert(renderer);
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1728,1084);
    assert(target && SDL_SetRenderTarget(renderer,target));
    canvas_w=1728;canvas_h=1084;
    SDL_SetRenderLogicalPresentation(renderer,1728,1084,SDL_LOGICAL_PRESENTATION_DISABLED);
    for(int i=0;i<count;i++) {
        combat_turn_based_whos_turn_set(allies[i]);
        sub_424070(allies[i],3,0,1);
        for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
        assert(combat_input_ready(allies[i]));
        selector=2;button_count=0;
        draw_tactical_hud();draw_selector();draw_binding_bar();
        int slot_buttons=0;bool hover=false;
        for(int j=0;j<button_count;j++) {
            if(buttons[j].action>=300 && buttons[j].action<310) {
                slot_buttons++;
                int slot=buttons[j].action-300;
                assert(binding_slots[slot].x==buttons[j].rect.x && binding_slots[slot].y==buttons[j].rect.y);
            }
            if(buttons[j].action==880+i) {
                mouse_x=buttons[j].rect.x+10;mouse_y=buttons[j].rect.y+10;hover=true;
            }
        }
        assert(slot_buttons==10 && hover);
        SDL_Event assign={.type=SDL_EVENT_KEY_DOWN};assign.key.scancode=SDL_SCANCODE_1;
        assert(binding_event(&assign));binding_tick();
        assert(modern_hotkey_get(0)->type==HOTKEY_SKILL && modern_hotkey_get(0)->data==i);
        selector=0;
    }
    int64_t item=item_wield_get(allies[0],ITEM_INV_LOC_ARMOR);
    assert(item);
    ObjectID item_id=obj_get_id(item);
    int item_location=item_inventory_location_get(item);
    combat_turn_based_whos_turn_set(allies[0]);
    for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
    assert(modern_hotkey_assign(1,HOTKEY_ITEM,0,item));
    assert(item_inventory_location_get(item)==item_location);
    assert(modern_hotkey_get(1)->item_obj.obj==item);
    int known_spell=-1;
    for(int spell=0;spell<SPELL_COUNT;spell++)if(spell_is_known(allies[0],spell)){known_spell=spell;break;}
    assert(known_spell>=0);
    selector=1;button_count=0;
    draw_tactical_hud();draw_selector();draw_binding_bar();
    bool spell_hover=false;
    for(int j=0;j<button_count;j++)if(buttons[j].action==1400+known_spell) {
        mouse_x=buttons[j].rect.x+10;mouse_y=buttons[j].rect.y+10;spell_hover=true;
    }
    assert(spell_hover);
    SDL_Event assign_spell={.type=SDL_EVENT_KEY_DOWN};assign_spell.key.scancode=SDL_SCANCODE_3;
    assert(panels_event(&assign_spell));binding_tick();
    SDL_Delay(100);
    assign_spell.type=SDL_EVENT_KEY_UP;
    assert(panels_event(&assign_spell));
    assert(selector==1 && modern_hotkey_get(2)->type==HOTKEY_SPELL && modern_hotkey_get(2)->data==known_spell);
    int ww,wh;assert(SDL_GetWindowSize(window,&ww,&wh));
    SDL_Event drag={.type=SDL_EVENT_MOUSE_BUTTON_DOWN};drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=mouse_x*ww/canvas_w;drag.button.y=mouse_y*wh/canvas_h;
    panels_event(&drag);
    assert(binding_pressed);
    drag.type=SDL_EVENT_MOUSE_MOTION;
    drag.motion.x=(binding_slots[3].x+10)*ww/canvas_w;
    drag.motion.y=(binding_slots[3].y+10)*wh/canvas_h;
    panels_event(&drag);
    assert(binding_moved);
    drag.type=SDL_EVENT_MOUSE_BUTTON_UP;drag.button.button=SDL_BUTTON_LEFT;
    drag.button.x=(binding_slots[3].x+10)*ww/canvas_w;
    drag.button.y=(binding_slots[3].y+10)*wh/canvas_h;
    assert(panels_event(&drag));binding_tick();
    assert(selector==1 && modern_hotkey_get(3)->type==HOTKEY_SPELL && modern_hotkey_get(3)->data==known_spell);
    selector=0;
    assert(sub_57F240(9)->type==hero.type && sub_57F240(9)->data==hero.data);
    assert(gamelib_save("Slot0094","Party hotkeys test"));
    assert(gamelib_load("Slot0094"));
    sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    for(int i=0;i<count;i++) {
        allies[i]=obj_pool_perm_lookup(owners[i]);assert(allies[i]);
        combat_turn_based_whos_turn_set(allies[i]);sub_424070(allies[i],3,0,1);
        for(int frame=0;frame<40;frame++){modern_ui_tick();SDL_Delay(25);}
        assert(modern_hotkey_get(0)->type==HOTKEY_SKILL && modern_hotkey_get(0)->data==i);
        TigMessage key={.type=TIG_MESSAGE_KEYBOARD};key.data.keyboard.scancode=SDL_SCANCODE_1;
        intgame_process_event(&key);
        assert(sub_579F90()==i);
        intgame_mode_set(INTGAME_MODE_MAIN);
    }
    combat_turn_based_whos_turn_set(allies[0]);
    assert(modern_hotkey_get(1)->item_obj.obj==obj_pool_perm_lookup(item_id));
    assert(modern_hotkey_get(2)->data==known_spell);
    assert(sub_57F240(9)->type==hero.type && sub_57F240(9)->data==hero.data);
    assert(gamelib_load(argv[2]));
    assert(!modern_hotkey_get(0) || modern_hotkey_get(0)==sub_57F240(0));
    combat_critter_activate_combat_mode(player_get_local_pc_obj());
    allies[0]=obj_pool_perm_lookup(owners[0]);assert(allies[0]);
    combat_turn_based_whos_turn_set(allies[0]);
    assert(!modern_hotkey_get(0));
    puts("PASS: companion HUD slots, menu digit assignments, actor isolation, inventory references, spells, full save/load, numbered activation and old saves");
    return 0;
}
