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
typedef struct ExpectedWall { int64_t obj; unsigned int sides; } ExpectedWall;
static void check_group(int64_t pc,const char* shot,int64_t camera)
{
    int count=combat_visibility_actors(NULL,0);
    int64_t fallback=combat_input_actor();
    int64_t* actors=malloc(sizeof(*actors)*(count?count:1));assert(actors);
    if(count)assert(combat_visibility_actors(actors,count)==count);
    else {count=1;actors[0]=fallback;}
    ExpectedWall* expected=NULL;int expected_count=0;
    for(int i=0;i<count;i++) {
        wallcheck_recalc(obj_field_int64_get(actors[i],OBJ_F_LOCATION));
        for(int w=0;w<dword_5E2E24;w++) {
            int64_t obj=stru_5E0E20[w].obj;int j=0;
            while(j<expected_count && expected[j].obj!=obj)j++;
            if(j==expected_count){expected=realloc(expected,sizeof(*expected)*(++expected_count));assert(expected);expected[j]=(ExpectedWall){obj,0};}
            expected[j].sides|=obj_field_int32_get(obj,OBJ_F_WALL_FLAGS)&6;
        }
    }
    wallcheck_ping(0);assert(wallcheck_observer_count==count);
    assert(dword_5E2E24==expected_count);
    for(int j=0;j<expected_count;j++) {
        int index;assert(sub_4386B0(expected[j].obj,&index));
        assert((obj_field_int32_get(expected[j].obj,OBJ_F_WALL_FLAGS)&6)==expected[j].sides);
    }
    assert(player_get_local_pc_obj()==pc);
    uint64_t before=faded_hash();
    for(int i=0;i<100;i++)wallcheck_ping(0);
    assert(faded_hash()==before);
    printf("Verified %s: %d actors, %d walls in the union\n",shot,count,dword_5E2E24);
    location_origin_set(camera);capture(shot);
    free(expected);free(actors);
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
    GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
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
    combat_turn_based_add_critter(companion);combat_turn_based_add_critter(enemy);
    check_group(pc,"all-actors-hero-turn",near_wall);assert(dword_5E2E24>0);
    uint64_t all=faded_hash();
    combat_turn_based_whos_turn_set(companion);wallcheck_ping(0);assert(faded_hash()==all);
    check_group(pc,"all-actors-companion-turn",near_wall);
    assert(!combat_player_controls(enemy));combat_turn_based_whos_turn_set(enemy);
    wallcheck_ping(0);assert(faded_hash()==all);
    check_group(pc,"all-actors-enemy-turn",near_wall);
    /* Moving someone whose turn has not started must also refresh the union. */
    combat_turn_based_whos_turn_set(pc);
    int64_t moved=LOCATION_MAKE(LOCATION_GET_X(near_wall)+3,LOCATION_GET_Y(near_wall));
    sub_43E770(enemy,moved,0,0);wallcheck_ping(0);
    check_group(pc,"enemy-moved-outside-turn",near_wall);
    int flags=obj_field_int32_get(enemy,OBJ_F_CRITTER_FLAGS);
    obj_field_int32_set(enemy,OBJ_F_CRITTER_FLAGS,flags|OCF_PARALYZED);
    check_group(pc,"paralyzed-enemy",near_wall);
    obj_field_int32_set(enemy,OBJ_F_CRITTER_FLAGS,flags);
    object_flags_set(enemy,OF_OFF);wallcheck_ping(0);
    check_group(pc,"enemy-removed",near_wall);object_flags_unset(enemy,OF_OFF);
    combat_critter_deactivate_combat_mode(pc);assert(!combat_turn_based_is_active());
    check_group(pc,"combat-ended",near_wall);assert(wallcheck_observer==pc);
    assert(modern_party_select(companion));wallcheck_ping(0);
    check_group(pc,"companion-outside-combat",near_wall);
    assert(wallcheck_observer==companion&&qword_5E0A08==obj_field_int64_get(companion,OBJ_F_LOCATION));
    sub_43E770(companion,moved,0,0);wallcheck_ping(0);
    assert(wallcheck_observer==companion&&qword_5E0A08==moved);
    modern_party_select(0);wallcheck_ping(0);assert(wallcheck_observer==pc&&qword_5E0A08==original);
    wallcheck_reset();assert(!dword_5E2E24&&!dword_5E2E28&&!wallcheck_observer_count);
    puts("PASS: simultaneous native wall union, stationary turns, off-turn movement, incapacitated/removed actors, combat exit and selected companion; player identity unchanged");
    return 0;
}
