/* Verify object approach and use in a disposable, marked game copy. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/descriptions.h"
#include "game/map.h"
#include "game/obj.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/modern_ui.c"
#include "../source/src/game/anim.c"
void sub_549A70(void);
static void pump(void)
{
    modern_ui_tick();tig_ping();gamelib_ping();modern_follow_camera_tick();
    TigMessage message;while(tig_message_dequeue(&message)==TIG_OK)intgame_process_event(&message);
    SDL_Delay(5);
}
int main(int argc,char** argv)
{
    if(argc!=2 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1920;init.height=1080;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    int start=map_by_type(MAP_TYPE_START_MAP);char* name;int64_t x,y;
    assert(map_get_name(start,&name)&&map_get_starting_location(start,&x,&y));
    assert(map_preprocess_mobile(name));char map_path[512];snprintf(map_path,sizeof(map_path),"maps\\%s",name);
    assert(map_open(map_path,"pathfinding-fixture",false));
    PlayerCreateInfo info;player_create_info_init(&info);info.basic_prototype=16066;info.loc=location_make(x,y);
    assert(player_obj_create_player(&info));
    ObjectList nearby;object_list_vicinity(info.obj,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next)object_flags_set(n->obj,OF_OFF);object_list_destroy(&nearby);

    sub_5412D0();in_game=true;enabled=true;
    intgame_mode_set(INTGAME_MODE_MAIN);object_flags_unset(info.obj,OF_OFF);
    settings_set_value(&settings,ALWAYS_RUN_KEY,1);
    printf("TEST pid=%ld SDL_window=%u isolated=%s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    int64_t cx=0,cy=0,actor=info.obj;
    for(int dy=-60;dy<=60 && !cx;dy+=12)for(int dx=-60;dx<=60 && !cx;dx+=12){
        bool clear=true;
        for(int j=-8;j<=8 && clear;j++)for(int i=-3;i<=3 && clear;i++){
            int64_t loc=location_make(x+dx+i,y+dy+j);
            if(tile_is_blocking(loc,false) || object_traversal_is_blocked(actor,loc,5,0,NULL)
                || object_traversal_is_blocked(actor,loc,3,0,NULL))clear=false;
        }
        if(clear){cx=x+dx;cy=y+dy;}
    }
    assert(cx);
    /* The chosen clear patch can be outside the starting viewport. Suppress
     * ambient combat throughout the area the free camera will reveal. */
    /* Native rectangle lists have room for three sectors per row. */
    for(int dy=-64;dy<=64;dy+=64)for(int dx=-64;dx<=64;dx+=64){
        LocRect bounds={cx+dx-32,cy+dy-32,cx+dx+32,cy+dy+32};
        object_list_rect(&bounds,OBJ_TM_NPC,&nearby);
        for(ObjectNode* n=nearby.head;n;n=n->next)object_flags_set(n->obj,OF_OFF);
        object_list_destroy(&nearby);
    }
    int64_t from=location_make(cx-1,cy),chest_loc=location_make(cx+1,cy),chest;
    assert(object_create(sub_4685A0(BP_WOOD_CHEST_1),chest_loc,&chest));
    int64_t walls[13];
    for(int j=-6;j<=6;j++){
        int64_t wall;assert(object_create(sub_4685A0(BP_LIGHT_WOOD_WALL),location_make(cx,cy+j),&wall));
        walls[j+6]=wall;
        tig_art_id_t art=obj_field_int32_get(wall,OBJ_F_CURRENT_AID);
        object_set_current_aid(wall,tig_art_id_rotation_set(art,5));
    }
    sub_43E770(actor,from,0,0);location_origin_set(from);iso_interface_refresh();
    /* A closed target portal is interactable without mutating its raw state. */
    int64_t door;assert(object_create(sub_4685A0(BP_LIGHT_WOOD_DOOR),location_make(cx-1,cy-3),&door));
    int portal_flags=obj_field_int32_get(door,OBJ_F_PORTAL_FLAGS);
    obj_field_int32_set(door,OBJ_F_PORTAL_FLAGS,portal_flags|OPF_LOCKED);
    int door_flags=obj_field_int32_get(door,OBJ_F_FLAGS);
    assert(anim_interaction_reachable(actor,location_make(cx-1,cy-2),door,2));
    assert(obj_field_int32_get(door,OBJ_F_FLAGS)==door_flags);
    object_flags_set(door,OF_OFF);
    AnimPath preview;assert(anim_preview_interaction_path(actor,chest,&preview));
    printf("PLAN steps=%d target_distance=%lld\n",preview.max,(long long)location_dist(from,chest_loc));
    assert(preview.max>8);
    assert(!anim_interaction_reachable(actor,location_make(cx,cy),chest,1));
    int flags=obj_field_int32_get(chest,OBJ_F_FLAGS);
    AnimGoalData goal={0};goal.params[AGDATA_RANGE_DATA].data=1;
    AnimRunInfo run={0};run.params[0].obj=actor;run.params[1].obj=chest;run.cur_stack_data=&goal;
    assert(sub_427990(&run));int8_t route[8];memcpy(route,run.path.rotations,8);
    assert(sub_4280D0(&run));assert(!memcmp(route,run.path.rotations,8));
    assert(obj_field_int32_get(chest,OBJ_F_FLAGS)==flags);
    AnimGoalData use;AnimID id;assert(sub_44D500(&use,actor,AG_USE_CONTAINER));
    use.params[AGDATA_TARGET_OBJ].obj=chest;assert(anim_goal_add(&use,&id));
    Uint64 started=SDL_GetTicks(),deadline=started+30000;
    while(!anim_interaction_reachable(actor,obj_field_int64_get(actor,OBJ_F_LOCATION),chest,1) && SDL_GetTicks()<deadline)pump();
    assert(anim_interaction_reachable(actor,obj_field_int64_get(actor,OBJ_F_LOCATION),chest,1));
    /* Let the use goal run after the last step; it opens the actual loot UI. */
    for(int i=0;i<200;i++)pump();
    printf("ARRIVAL elapsed_ms=%llu mode=%d distance=%lld\n",(unsigned long long)(SDL_GetTicks()-started),intgame_mode_get(),
        (long long)location_dist(obj_field_int64_get(actor,OBJ_F_LOCATION),chest_loc));
    assert(obj_field_int32_get(chest,OBJ_F_FLAGS)==flags);
    assert(intgame_mode_get()==INTGAME_MODE_LOOT);

    modern_inventory_action(0);assert(intgame_mode_get()==INTGAME_MODE_MAIN);
    sub_423FF0(actor);sub_43E770(actor,from,0,0);
    /* A shorter barrier fits into one ordinary tactical turn. */
    for(int j=-6;j<=6;j++)if(j < -2 || j > 2)object_flags_set(walls[j+6],OF_OFF);
    combat_tactical_enable();combat_critter_activate_combat_mode(actor);
    assert(combat_turn_based_is_active());combat_turn_based_whos_turn_set(actor);
    assert(anim_preview_interaction_path(actor,chest,&preview));
    int ap_before=combat_action_points_get();
    assert(combat_check_use_obj(actor,chest));
    assert(combat_required_action_points_get()==2+preview.max);
    printf("COMBAT_PLAN steps=%d ap=%d required=%d\n",preview.max,ap_before,combat_required_action_points_get());
    assert(ap_before>=2+preview.max);
    assert(sub_44D500(&use,actor,AG_USE_CONTAINER));use.params[AGDATA_TARGET_OBJ].obj=chest;
    assert(anim_goal_add(&use,&id));turn_on_running(id);
    started=SDL_GetTicks();deadline=started+20000;
    while(intgame_mode_get()!=INTGAME_MODE_LOOT && SDL_GetTicks()<deadline){
        pump();assert(combat_turn_based_is_active());
    }
    assert(intgame_mode_get()==INTGAME_MODE_LOOT);
    assert(anim_interaction_reachable(actor,obj_field_int64_get(actor,OBJ_F_LOCATION),chest,1));
    printf("COMBAT_USE elapsed_ms=%llu ap_before=%d ap_after=%d\n",(unsigned long long)(SDL_GetTicks()-started),ap_before,combat_action_points_get());
    assert(combat_action_points_get()==ap_before-preview.max-2);
    puts("PASS: native wall-backed chest, actual exploration and tactical approach/use, full route AP estimate matches consumption");
    return 0;
}
