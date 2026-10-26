/* Link in place of main.c and wallcheck.c; only use an isolated marked copy.
 * Usage: check-actor-walls GAME CAPTURE_DIRECTORY [SAVE_SLOT] */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/anim.h"
#include "game/tile.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/follower_ui.h"
#include "../source/src/game/wallcheck.c"
void sub_549A70(void);
static const char* capture_dir;
static SDL_Renderer* renderer;
static uint64_t faded_hash(void)
{
    uint64_t hash=1469598103934665603ULL;
    for(int i=0;i<dword_5E2E24;i++){
        int64_t obj=stru_5E0E20[i].obj;
        hash=(hash^obj)*1099511628211ULL;
        hash=(hash^obj_field_int32_get(obj,OBJ_F_WALL_FLAGS))*1099511628211ULL;
    }
    return hash;
}
static void capture(const char* name)
{
    modern_ui_tick();iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface);
    char path[4096];snprintf(path,sizeof(path),"%s/%s.png",capture_dir,name);
    assert(SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
}
static void check_actor(int64_t actor,int64_t pc,const char* shot)
{
    int64_t loc=obj_field_int64_get(actor,OBJ_F_LOCATION);
    roof_recalc(loc);wallcheck_recalc(loc);uint64_t expected=faded_hash();
    roof_recalc(obj_field_int64_get(pc,OBJ_F_LOCATION));wallcheck_recalc(obj_field_int64_get(pc,OBJ_F_LOCATION));
    combat_turn_based_whos_turn_set(actor);assert(combat_turn_based_whos_turn_get()==actor);
    wallcheck_ping(0);assert(wallcheck_observer==actor&&qword_5E0A08==loc);
    assert(faded_hash()==expected);assert(player_get_local_pc_obj()==pc);
    printf("Verified %s: %d faded walls\n",shot,dword_5E2E24);
    location_origin_set(loc);capture(shot);
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
    assert(tig_video_renderer_get(&renderer)==TIG_OK);
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(SDL_GetRenderWindow(renderer)),argv[1]);

    capture_dir=argv[2];
    assert(gamelib_load(argc==4?argv[3]:"Slot0002"));sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();



    int64_t original=obj_field_int64_get(pc,OBJ_F_LOCATION),party[8],companion=0,enemy=0;
    int count=modern_followers_get(party,8);
    for(int i=0;i<count;i++)if(party[i]!=pc){companion=party[i];break;}
    ObjectList npcs;object_list_vicinity(pc,OBJ_TM_NPC,&npcs);
    for(ObjectNode* n=npcs.head;n;n=n->next)if(critter_pc_leader_get(n->obj)!=pc&&critter_is_active(n->obj)){enemy=n->obj;break;}
    object_list_destroy(&npcs);assert(companion&&enemy);
    int64_t near_wall=0;
    for(int dx=-20;dx<=20&&!near_wall;dx++)for(int dy=-20;dy<=20&&!near_wall;dy++){
        if(abs(dx)+abs(dy)<8)continue;
        int64_t loc=LOCATION_MAKE(LOCATION_GET_X(original)+dx,LOCATION_GET_Y(original)+dy);
        if(tile_is_blocking(loc,false)||!roof_is_covered_loc(loc,false))continue;
        wallcheck_recalc(loc);
        if(dword_5E2E24>0)near_wall=loc;
    }
    assert(near_wall);wallcheck_reset();
    sub_43E770(companion,near_wall,0,0);
    sub_43E770(enemy,LOCATION_MAKE(LOCATION_GET_X(near_wall)+1,LOCATION_GET_Y(near_wall)),0,0);
    wallcheck_ping(0);assert(wallcheck_observer==pc);
    combat_critter_activate_combat_mode(pc);assert(combat_turn_based_is_active());
    check_actor(pc,pc,"hero");
    check_actor(companion,pc,"companion");assert(dword_5E2E24>0);
    assert(!combat_player_controls(enemy));check_actor(enemy,pc,"enemy");
    int64_t moved=near_wall;sub_43E770(enemy,moved,0,0);
    wallcheck_ping(0);assert(wallcheck_observer==enemy&&qword_5E0A08==moved);
    uint64_t actual=faded_hash();wallcheck_recalc(moved);assert(faded_hash()==actual);
    location_origin_set(moved);capture("enemy-moved");
    check_actor(companion,pc,"companion-again");
    combat_critter_deactivate_combat_mode(pc);assert(!combat_turn_based_is_active());
    wallcheck_ping(0);assert(wallcheck_observer==pc&&qword_5E0A08==original);
    location_origin_set(original);capture("combat-ended");
    assert(modern_party_select(companion));wallcheck_ping(0);
    assert(wallcheck_observer==companion&&qword_5E0A08==obj_field_int64_get(companion,OBJ_F_LOCATION));
    location_origin_set(near_wall);capture("companion-outside-combat");
    int64_t selected_moved=LOCATION_MAKE(LOCATION_GET_X(near_wall)+1,LOCATION_GET_Y(near_wall));
    sub_43E770(companion,selected_moved,0,0);wallcheck_ping(0);
    assert(wallcheck_observer==companion&&qword_5E0A08==selected_moved);
    modern_party_select(0);wallcheck_ping(0);assert(wallcheck_observer==pc&&qword_5E0A08==original);
    puts("PASS: native wall fading follows stationary hero/companion/nonparty turns and movement; restores hero after combat without changing player identity");
    return 0;
}
