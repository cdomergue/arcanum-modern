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


    modern_ui_tick();hud=true;
    /* Render complete HUD/selector screens, including footer and partial roster. */
    int64_t members[128];int count=modern_followers_get(members,128);
    assert(count>0);
    int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON);assert(weapon);
    obj_field_int32_set(weapon,OBJ_F_ITEM_FLAGS,obj_field_int32_get(weapon,OBJ_F_ITEM_FLAGS)|OIF_IDENTIFIED);
    obj_field_int32_set(weapon,OBJ_F_ITEM_MAGIC_TECH_COMPLEXITY,10);
    obj_field_int32_set(weapon,OBJ_F_ITEM_SPELL_1,42);
    selector_spells_collect(pc);bool found=false;
    for(int page=0;page<selector_spell_pages;page++){
        tactical_spell_page=page;selector_spells_collect(pc);
        for(int i=0;i<selector_spell_count;i++)if(selector_spells[i].item==weapon&&selector_spells[i].spell==42)found=true;
    }
    assert(found);
    for(int pass=0;pass<5;pass++) {
        selector=pass==3?1:0;
party_count=7;party_page=pass==1?1:0;
        for(int i=0;i<7;i++)party_members[i]=members[i%count];
        SDL_SetRenderDrawColor(renderer,28,29,26,255);SDL_RenderClear(renderer);
        button_count=0;drawing_hud=true;draw_hud();drawing_hud=false;
        if(!selector)draw_party();else draw_tactical_spells();
        if(pass==1){float y=88+party_rows()*108+12;assert(party_region(20,y));assert(hit(20,y)==1930);}
        if(pass==2 || pass==4){drawing_hud=true;float x=(canvas_w-980)/2,y=canvas_h-126;
            if(pass==4){make_font(&fonts[0],book_font_data,26);mouse_x=x+40;mouse_y=y+50;}
            gauge(x+27,y+9,pass==4?1234:250,pass==4?9999:250,false);gauge(x+915,y+9,pass==4?-12:250,250,true);drawing_hud=false;}
        char path[4096];snprintf(path,sizeof(path),"%s-%d.png",argv[3],pass);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
        if(pass==1) {
            int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
            SDL_Event event={0};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;event.button.down=true;
            event.button.button=SDL_BUTTON_LEFT;event.button.x=20*ww/canvas_w;
            event.button.y=(88+party_rows()*108+12)*wh/canvas_h;
            assert(party_event(&event));SDL_Delay(120);assert(party_page==1);
            event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;assert(party_event(&event));
            assert(pending_action==1930);activate(pending_action);pending_action=-1;assert(party_page==0);
        }
    }
    selector=1;selector_spells_collect(pc);
    for(int i=0;i<selector_spell_count;i++)if(selector_spells[i].item==weapon){
        int spell=selector_spells[i].spell;assert(selector_spell_action(1540+i));
        assert(modern_spell_selected()==spell);spell_ui_cancel();break;
    }
    puts("PASS: real inventory item spells, partial roster footer, full HUD and spell panel captures");
    return 0;
}
