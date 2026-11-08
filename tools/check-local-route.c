/* Verify local-map long-leg planning and travel in a marked disposable copy. */
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

void sub_549A70(void);
static void pump(void)
{
    modern_ui_tick();tig_ping();gamelib_ping();modern_follow_camera_tick();
    TigMessage message;while(tig_message_dequeue(&message)==TIG_OK)intgame_process_event(&message);
    SDL_Delay(5);
}
int64_t check_local_route_plan(void);
void check_local_route_notes(void);
void check_local_route_start(void);
bool check_local_route_active(void);
int main(int argc,char** argv)
{
    if(argc!=2 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
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
    world_zoom=.5f;world_zoom_sync();
    settings_set_value(&settings,ALWAYS_RUN_KEY,1);
    /* Disable ambient combat in the entire expanded viewport of this fixture. */
    object_list_vicinity(info.obj,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next)if(n->obj!=ally)object_flags_set(n->obj,OF_OFF);
    object_list_destroy(&nearby);
    assert(hrp_iso_window_width_get()>init.width);
    check_local_route_notes();
    int64_t destination=check_local_route_plan();
    ModernMap state;assert(modern_map_state(&state));
    int memo_x,memo_y;assert(modern_map_local_point(0,&memo_x,&memo_y));
    notebook_sync();map_memo_count=3;memos_visible=true;
    map_memos[0]=(MapMemo){.mode=state.mode,.map=state.map,.townmap=state.townmap,.x=memo_x,.y=memo_y};
    strcpy(map_memos[0].text,"Fixture current city");
    map_memos[1]=map_memos[0];map_memos[1].townmap=state.townmap+10000;strcpy(map_memos[1].text,"Fixture other city");
    map_memos[2]=map_memos[0];map_memos[2].townmap=0;strcpy(map_memos[2].text,"Fixture legacy unassigned");
    assert(notebook_save());notebook_identity[0]=0;notebook_sync();assert(map_memo_count==3);
    skip_map_generation=true;
    int old_w,old_h;SDL_RendererLogicalPresentation old_mode;
    SDL_GetRenderLogicalPresentation(renderer,&old_w,&old_h,&old_mode);
    SDL_SetRenderLogicalPresentation(renderer,1600,900,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    button_count=panel_region_count=0;SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);draw_map();
    map_camera.scale=map_camera.min_scale;map_camera.x=map_camera.y=0;draw_map();
    bool own=false;
    for(int i=0;i<button_count;i++){
        assert(buttons[i].action!=2201 && buttons[i].action!=2202);
        if(buttons[i].action==2200){own=true;mouse_x=buttons[i].rect.x+12;mouse_y=buttons[i].rect.y+12;}
    }
    assert(own);button_count=panel_region_count=0;
    SDL_SetRenderViewport(renderer,NULL);SDL_SetRenderClipRect(renderer,NULL);
    SDL_SetRenderLogicalPresentation(renderer,1600,900,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    draw_map();assert(memo_hover==0);
    puts("PERSONAL_NOTES current_city=1 foreign_city=0 legacy_visible=0 legacy_preserved=1");
    SDL_Surface* capture=SDL_RenderReadPixels(renderer,NULL);assert(capture&&SDL_SavePNG(capture,"local-route-reduced.png"));SDL_DestroySurface(capture);
    SDL_SetRenderLogicalPresentation(renderer,old_w,old_h,old_mode);
    ModernLocalMap local;assert(modern_map_local_state(&local)&&local.count==1);
    check_local_route_start();
    Uint64 started=SDL_GetTicks(),deadline=started+60000;
    while(check_local_route_active()&&SDL_GetTicks()<deadline)pump();
    printf("LOCAL_ARRIVAL elapsed_ms=%llu remaining=%lld active=%d\n",(unsigned long long)(SDL_GetTicks()-started),
        (long long)location_dist(obj_field_int64_get(info.obj,OBJ_F_LOCATION),destination),check_local_route_active());
    assert(!check_local_route_active()&&obj_field_int64_get(info.obj,OBJ_F_LOCATION)==destination);
    puts("PASS: local map uses one long validated waypoint and native route travel reaches it");
    return 0;
}
