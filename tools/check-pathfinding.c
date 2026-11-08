/* Verify distant zoomed-out ground clicks in a disposable, marked game copy. */
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
#include "../source/src/game/path.c"
void sub_549A70(void);
static void pump(void)
{
    modern_ui_tick();tig_ping();gamelib_ping();modern_follow_camera_tick();
    TigMessage message;while(tig_message_dequeue(&message)==TIG_OK)intgame_process_event(&message);
    SDL_Delay(5);
}
static void distant_click(int64_t actor)
{
    int64_t from=obj_field_int64_get(actor,OBJ_F_LOCATION),to=0;
    location_origin_set(from);iso_interface_refresh();
    for(int distance=40;distance>=34 && !to;distance-=2)for(int direction=1;direction<8 && !to;direction+=2){
        int64_t candidate;if(!location_in_range(from,direction,distance,&candidate))continue;
        int64_t x,y;location_xy(candidate,&x,&y);x+=40;y+=20;
        float px=x*canvas_w/hrp_iso_window_width_get(),py=y*canvas_h/hrp_iso_window_height_get();
        if(px<120 || px>canvas_w-120 || py<110 || py>canvas_h-180 || hud_region(px,py))continue;
        TargetDescriptor td;
        if(!target_pick_at_screen_xy_ex((int)x,(int)y,&td,TGT_OBJECT|TGT_OBJ_NO_T_WALL|TGT_TILE,false) || !td.is_loc || td.loc!=candidate)continue;
        AnimPath preview;
        if(!anim_preview_path(actor,candidate,false,&preview))continue;
        to=candidate;
        printf("CLICK actor=%llu distance=%lld canvas=(%.0f,%.0f) world_view=%dx%d\n",(unsigned long long)actor,(long long)location_dist(from,to),px,py,hrp_iso_window_width_get(),hrp_iso_window_height_get());
        tig_mouse_set_position((int)x,(int)y);
        TigMessage pending;while(tig_message_dequeue(&pending)==TIG_OK){}
        TigMessage click={0};click.type=TIG_MESSAGE_MOUSE;click.data.mouse.x=(int)x;click.data.mouse.y=(int)y;
        click.data.mouse.event=TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;intgame_process_event(&click);
        SDL_Delay(80);
        click.data.mouse.event=TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;intgame_process_event(&click);
    }
    assert(to && location_dist(from,to)>32);
    int64_t camera_x,camera_y,after_x,after_y;
    location_origin_get(&camera_x,&camera_y);
    int pan_frames=0;int64_t max_pan=0,previous_x=camera_x,previous_y=camera_y;
    Uint64 started=SDL_GetTicks(),deadline=started+45000;
    while(obj_field_int64_get(actor,OBJ_F_LOCATION)!=to && SDL_GetTicks()<deadline){
        pump();
        int64_t frame_x,frame_y;location_origin_get(&frame_x,&frame_y);
        int64_t change=llabs(frame_x-previous_x)+llabs(frame_y-previous_y);
        if(change){pan_frames++;if(change>max_pan)max_pan=change;}
        previous_x=frame_x;previous_y=frame_y;
        int64_t dx,dy;
        location_calc_dist_from_screen_center(obj_field_int64_get(actor,OBJ_F_LOCATION),&dx,&dy);
        dx-=obj_field_int32_get(actor,OBJ_F_OFFSET_X);dy-=obj_field_int32_get(actor,OBJ_F_OFFSET_Y);
        assert(llabs(dx)<hrp_iso_window_width_get()/2-40 && llabs(dy)<hrp_iso_window_height_get()/2-60);
    }
    location_origin_get(&after_x,&after_y);
    assert(after_x!=camera_x || after_y!=camera_y);
    assert(pan_frames==1);
    printf("EDGE_JUMP frames=%d max_frame_shift=%lld\n",pan_frames,(long long)max_pan);
    int64_t dx,dy;location_calc_dist_from_screen_center(to,&dx,&dy);
    assert(dx || dy); /* Free mode keeps the actor visible without centering. */
    printf("FREE_CAMERA actor=%llu shift=%lld,%lld remaining_offset=%lld,%lld\n",(unsigned long long)actor,
        (long long)(after_x-camera_x),(long long)(after_y-camera_y),(long long)dx,(long long)dy);
    printf("ARRIVAL actor=%llu elapsed_ms=%llu remaining=%lld\n",(unsigned long long)actor,(unsigned long long)(SDL_GetTicks()-started),(long long)location_dist(obj_field_int64_get(actor,OBJ_F_LOCATION),to));
    if(obj_field_int64_get(actor,OBJ_F_LOCATION)!=to){
        AnimID id;AnimRunInfo* run;
        if(sub_423300(actor,&id)&&anim_id_to_run_info(&id,&run))
            fprintf(stderr,"STOP goal=%d target_distance=%lld combat=%d turn_based=%d\n",run->goals[0].type,
                (long long)location_dist(to,run->goals[0].params[AGDATA_TARGET_TILE].loc),combat_critter_is_combat_mode_active(actor),combat_turn_based_is_active());
    }
    assert(obj_field_int64_get(actor,OBJ_F_LOCATION)==to);
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
    int64_t ally;assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(x+4,y),&ally));
    assert(critter_follow(ally,info.obj,true));sub_5412D0();in_game=true;enabled=true;
    intgame_mode_set(INTGAME_MODE_MAIN);object_flags_unset(info.obj,OF_OFF);
    location_origin_set(info.loc);iso_interface_refresh();modern_ui_tick();
    printf("TEST pid=%ld SDL_window=%u isolated=%s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    follow_camera=false;world_zoom=.5f;world_zoom_sync();
    settings_set_value(&settings,ALWAYS_RUN_KEY,1);
    /* Disable ambient combat in the entire expanded viewport of this fixture. */
    object_list_vicinity(info.obj,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next)if(n->obj!=ally)object_flags_set(n->obj,OF_OFF);
    object_list_destroy(&nearby);
    assert(hrp_iso_window_width_get()>init.width);
    distant_click(info.obj);
    sub_43E770(ally,obj_field_int64_get(info.obj,OBJ_F_LOCATION),0,0);
    assert(modern_party_select(ally));
    distant_click(ally);
    assert(modern_party_select(0));
    puts("PASS: far zoomed-out ground clicks route hero and selected companion to destination beyond 32 tiles");
    return 0;
}
