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
#include "../source/src/game/anim.c"

int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    printf("Companion trap test PID %ld, isolated directory %s\n",(long)getpid(),argv[1]);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
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


    int64_t pc=player_get_local_pc_obj(),party[8],companion=0,enemy=0;
    int count=modern_followers_get(party,8);
    for(int i=0;i<count;i++)if(party[i]!=pc){companion=party[i];break;}
    ObjectList npcs;object_list_vicinity(pc,OBJ_TM_NPC,&npcs);
    for(ObjectNode* n=npcs.head;n;n=n->next)if(!critter_pc_leader_get(n->obj)&&critter_is_active(n->obj)){enemy=n->obj;break;}
    object_list_destroy(&npcs);assert(companion&&enemy);
    assert(path_set_limit(34));
    int64_t from=obj_field_int64_get(pc,OBJ_F_LOCATION);
    int64_t to=LOCATION_MAKE(LOCATION_GET_X(from)+4,LOCATION_GET_Y(from));
    int64_t trapped=LOCATION_MAKE(LOCATION_GET_X(from)+2,LOCATION_GET_Y(from));
    int64_t trap;
    assert(object_create(sub_4685A0(BP_MECHANICAL_TRAP),trapped,&trap));
    object_flags_unset(trap,OF_TRAP_PC|OF_TRAP_SPOTTED|OF_OFF);
    int8_t steps[128];
    // Ignore the loaded map's physical obstacles to isolate trap routing.
    // The real straight-path shortcut and A* still run.
    PathCreateInfo path={0};path.obj=companion;path.from=from;path.to=to;
    path.rotations=steps;path.max_rotations=128;path.flags=PATH_FLAG_0x0040;
    assert(!trap_party_avoids_location(companion,trapped));
    int n=path_make(&path);assert(n==4);
    object_flags_set(trap,OF_TRAP_SPOTTED);
    assert(trap_party_avoids_location(companion,trapped));
    n=path_make(&path);assert(n>0);
    int64_t loc=from;
    for(int i=0;i<n;i++){assert(location_in_dir(loc,steps[i],&loc));assert(loc!=trapped);}
    assert(loc==to);
    puts("PASS: detected trap detour, unknown traps remain unknown");

    path.to=trapped;assert(path_make(&path)==0);
    // Approaching to disarm stops on the neighboring tile, never on the trap.
    for(int search=0;search<2;search++){
        path.flags=PATH_FLAG_0x0040|PATH_FLAG_0x0001|(search?PATH_FLAG_0x0200:0);
        n=path_make(&path);assert(n>0);loc=from;
        for(int i=0;i<n;i++){assert(location_in_dir(loc,steps[i],&loc));assert(loc!=trapped);}
        assert(location_dist(loc,trapped)==1);
    }
    path.flags=PATH_FLAG_0x0040;path.to=to;
    puts("PASS: direct entry refused, adjacent approach for disarming preserved");
    path.obj=pc;n=path_make(&path);assert(n>0);loc=from;
    for(int i=0;i<n;i++){assert(location_in_dir(loc,steps[i],&loc));assert(loc!=trapped);}
    assert(loc==to);
    path.obj=enemy;assert(path_make(&path)==4);path.obj=companion;
    assert(trap_party_avoids_location(pc,trapped));
    assert(!trap_party_avoids_location(enemy,trapped));
    // A second unknown trap on the same tile must not hide the known one.
    int64_t second;assert(object_create(sub_4685A0(BP_MECHANICAL_TRAP),trapped,&second));
    object_flags_unset(second,OF_TRAP_PC|OF_TRAP_SPOTTED|OF_OFF);
    assert(trap_party_avoids_location(companion,trapped));
    object_destroy(second);

    // Check a previously computed movement step after a trap is revealed.
    AnimRunInfo run={0};AnimGoalData goal={0};run.cur_stack_data=&goal;
    run.path.max=2;run.path.curr=0;run.path.rotations[0]=location_rot(from,trapped);
    int64_t adjacent;assert(location_in_dir(from,run.path.rotations[0],&adjacent));
    int64_t step_trap;assert(object_create(sub_4685A0(BP_MECHANICAL_TRAP),adjacent,&step_trap));
    object_flags_set(step_trap,OF_TRAP_SPOTTED);object_flags_unset(step_trap,OF_OFF);
    assert(sub_427110(&run,companion,from));
    assert(sub_427110(&run,pc,from));
    // Rechecking the same queued step must still block it after detection.
    assert(sub_427110(&run,pc,from));
    puts("PASS: cached movement stops before a newly detected trap");
    object_destroy(step_trap);

    // If every exit is known to be trapped, refuse the path, never force it.
    int64_t ring[8];
    for(int r=0;r<8;r++){
        assert(location_in_dir(from,r,&adjacent));
        assert(object_create(sub_4685A0(BP_MECHANICAL_TRAP),adjacent,&ring[r]));
        object_flags_set(ring[r],OF_TRAP_SPOTTED);object_flags_unset(ring[r],OF_OFF);
    }
    assert(path_make(&path)==0);
    path.obj=pc;assert(path_make(&path)==0);
    // An object interaction stops adjacent to its target, but cannot cross
    // the known traps enclosing the hero to get there.
    path.flags|=PATH_FLAG_0x0001;assert(path_make(&path)==0);
    // Exercise the actual queued chest interaction, not just a path query.
    int64_t chest;assert(object_create(sub_4685A0(BP_WOOD_CHEST_1),to,&chest));
    AnimGoalData use={0};AnimID use_id;
    assert(sub_44D500(&use,pc,AG_USE_CONTAINER));
    use.params[AGDATA_TARGET_OBJ].obj=chest;
    assert(anim_goal_add(&use,&use_id));
    for(int tick=0;tick<200;tick++){
        tig_timestamp_t now;tig_timer_now(&now);timeevent_ping(now);
        assert(obj_field_int64_get(pc,OBJ_F_LOCATION)==from);
        if(tick>5&&!sub_423300(pc,NULL))break;
        SDL_Delay(10);
    }
    assert(!sub_423300(pc,NULL));
    object_destroy(chest);
    puts("PASS: queued chest interaction terminates without walking through known traps");
    path.flags=PATH_FLAG_0x0040;path.obj=companion;
    for(int r=0;r<8;r++)object_destroy(ring[r]);
    object_flags_set(trap,OF_OFF);
    assert(!trap_party_avoids_location(companion,trapped));assert(path_make(&path)==4);
    object_flags_unset(trap,OF_OFF|OF_TRAP_SPOTTED);object_flags_set(trap,OF_TRAP_PC);
    assert(trap_party_avoids_location(companion,trapped));
    combat_critter_activate_combat_mode(pc);assert(combat_turn_based_is_active());
    combat_turn_based_add_critter(companion);combat_turn_based_whos_turn_set(companion);
    int ap=combat_action_points_get();
    n=path_make(&path);assert(n>0);loc=from;
    for(int i=0;i<n;i++){assert(location_in_dir(loc,steps[i],&loc));assert(loc!=trapped);}
    assert(loc==to&&combat_action_points_get()==ap);
    puts("PASS: controlled companion avoids traps in combat without spending AP during planning");
    object_destroy(trap);assert(path_make(&path)==4);
    puts("PASS: no safe route stops movement; disabled/removed traps free the route; player traps avoided");
    puts("PASS: hero cannot resume through detected traps; unrelated NPC behavior preserved");
    return 0;
}
