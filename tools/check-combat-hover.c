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

static void point_at(int64_t object)
{
    location_origin_set(obj_field_int64_get(object,OBJ_F_LOCATION));
    TigRect rect;object_get_rect(object,0,&rect);
    for(int y=rect.y;y<rect.y+rect.height;y+=2)for(int x=rect.x;x<rect.x+rect.width;x+=2) {
        TargetDescriptor td;
        if(target_pick_at_screen_xy_ex(x,y,&td,TGT_OBJECT|TGT_OBJ_NO_T_WALL,true) && !td.is_loc && td.obj==object) {
            tig_mouse_set_position(x,y);
            mouse_x=(float)x*canvas_w/hrp_iso_window_width_get();
            mouse_y=(float)y*canvas_h/hrp_iso_window_height_get();
            return;
        }
    }
    fprintf(stderr,"Cannot point at object %lld\n",(long long)object);abort();
}

static void inspect(int64_t object,const char* prefix,const char* suffix)
{
    point_at(object);
    object_hover_obj_set(OBJ_HANDLE_NULL); /* Simulate missing/rejected idle events. */
    uint64_t flags=target_flags_get();
    int ap=combat_action_points_get();
    char name[2000];object_examine(object,player_get_local_pc_obj(),name);
    const ModernDetails* d=modern_hover_details();
    assert(d && !strcmp(d->title,name) && d->bar_count==2 && (d->portrait>=0 || d->icon>=0));
    assert(object_hover_obj_get()==OBJ_HANDLE_NULL && target_flags_get()==flags && combat_action_points_get()==ap);
    /* Repeated reads must leave attack targeting and command state alone. */
    for(int i=0;i<20;i++)assert(modern_hover_details());
    if(suffix) {
        button_count=0;SDL_SetRenderDrawColor(renderer,20,20,20,255);SDL_RenderClear(renderer);
        draw_world_hover();
        if(!strcmp(suffix,"enemy")) {
            tactical_preview=(ModernCombatPreview){.actor=combat_input_actor(),.target=object,.kind=1,
                .cost=4,.chance=80,.in_range=true,.reachable=true,.resources=true,.affordable=true};
            tactical_has_preview=true;mouse_x=canvas_w/2;mouse_y=canvas_h-150;
            draw_tactical_preview();tactical_has_preview=false;
        }
        draw_tactical_hud();draw_party();
        char path[4096];snprintf(path,sizeof(path),"%s-%s.png",prefix,suffix);
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
        assert(surface && SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
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
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1600,900);
    assert(target && SDL_SetRenderTarget(renderer,target));update_geometry();
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);

    int64_t allies[8];int count=modern_followers_get(allies,8);assert(count>=2);
    int64_t enemy=0;ObjectList nearby;object_list_vicinity(pc,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next)if(critter_pc_leader_get(n->obj)!=pc && critter_is_active(n->obj)){enemy=n->obj;break;}
    object_list_destroy(&nearby);assert(enemy);
    combat_critter_activate_combat_mode(pc);assert(combat_turn_based_is_active());
    modern_ui_tick();tactical_camera.moving=false;tactical_camera.unlock=0;
    /* Inspection includes allies without weakening the native attack mask. */
    target_flags_set(TGT_OBJECT|TGT_OBJ_NO_SELF|TGT_NON_PARTY_CRITTERS|TGT_TILE);
    combat_turn_based_whos_turn_set(pc);
    inspect(enemy,argv[3],"enemy");inspect(allies[1],argv[3],"ally");inspect(pc,argv[3],NULL);
    combat_turn_based_whos_turn_set(allies[0]);assert(combat_input_actor()==allies[0]);
    inspect(enemy,argv[3],"companion-turn");inspect(allies[1],argv[3],NULL);
    combat_turn_based_whos_turn_set(enemy);
    inspect(allies[1],argv[3],"enemy-turn");inspect(enemy,argv[3],NULL);
    intgame_mode_set(INTGAME_MODE_SPELL);
    inspect(allies[1],argv[3],NULL);inspect(enemy,argv[3],NULL);
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(modern_world_resize(1200,675,.5f,.5f));
    inspect(allies[1],argv[3],NULL);inspect(enemy,argv[3],NULL);
    point_at(allies[1]);
    const ModernDetails* d=modern_hover_details();int old=d->bar_values[0];
    int damage=obj_field_int32_get(allies[1],OBJ_F_HP_DAMAGE);
    obj_field_int32_set(allies[1],OBJ_F_HP_DAMAGE,damage+5);SDL_Delay(110);
    d=modern_hover_details();assert(d && d->bar_values[0]<old);
    obj_field_int32_set(allies[1],OBJ_F_HP_DAMAGE,damage);
    puts("PASS: enemy, ally and self inspection on player, companion and enemy turns; portrait, live gauges, spell mode, zoom, unchanged attack targeting/AP");
    return 0;
}
