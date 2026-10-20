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


    assert(modern_party_select(OBJ_HANDLE_NULL));
    for(int i=0;i<40;i++){modern_ui_tick();SDL_Delay(25);}
    int64_t worn[2][9];
    for(int i=0;i<9;i++) {
        worn[0][i]=item_wield_get(pc,FIRST_WEAR_INV_LOC+i);
        worn[1][i]=item_wield_get(allies[0],FIRST_WEAR_INV_LOC+i);
    }
    assert(inven_ui_open(pc,allies[0],INVEN_UI_MODE_BARTER));
    ModernInventory inv;assert(modern_inventory_state(&inv) && inv.transfer_all);
    int gold=item_gold_get(pc)+item_gold_get(allies[0]);assert(item_gold_get(pc)>0);
    assert(panels_action(2408));
    assert(item_gold_get(pc)==0 && item_gold_get(allies[0])==gold);
    assert(panels_action(2407));
    assert(item_gold_get(pc)==gold && item_gold_get(allies[0])==0);
    for(int i=0;i<9;i++) {
        assert(item_wield_get(pc,FIRST_WEAR_INV_LOC+i)==worn[0][i]);
        assert(item_wield_get(allies[0],FIRST_WEAR_INV_LOC+i)==worn[1][i]);
    }
    canvas_w=1728;canvas_h=1084;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1728,1084);
    assert(target && SDL_SetRenderTarget(renderer,target));
    SDL_SetRenderLogicalPresentation(renderer,1728,1084,SDL_LOGICAL_PRESENTATION_DISABLED);
    button_count=panel_region_count=0;draw_inventory();
    int bulk=0;
    for(int i=0;i<button_count;i++)if(buttons[i].action==2407 || buttons[i].action==2408) {
        bulk++;
        assert(buttons[i].rect.y>=inventory_detail_rect.y+inventory_detail_rect.h);
        for(int j=0;j<button_count;j++)if(i!=j)assert(!SDL_HasRectIntersectionFloat(&buttons[i].rect,&buttons[j].rect));
    }
    assert(bulk==2);
    const char* capture=SDL_getenv("ARCANUM_TEST_CAPTURE");
    if(capture) {
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
        assert(surface && SDL_SavePNG(surface,capture));SDL_DestroySurface(surface);
    }
    inven_ui_destroy();
    assert(inven_ui_open(pc,allies[0],INVEN_UI_MODE_LOOT));
    assert(modern_inventory_state(&inv) && inv.transfer_all);
    assert(panels_action(2408));assert(item_gold_get(pc)==0 && item_gold_get(allies[0])==gold);
    assert(panels_action(2407));assert(item_gold_get(pc)==gold && item_gold_get(allies[0])==0);
    inven_ui_destroy();
    puts("PASS: native companion/loot transfers in both directions, whole gold stacks, equipped items and non-overlapping buttons");
    return 0;
}
