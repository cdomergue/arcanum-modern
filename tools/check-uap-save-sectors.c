#include <stdio.h>
#include "ui/iso.h"
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
#include "game/object.h"
#include "game/location.h"
#include "game/sector.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "game/map.h"
#include "game/jumppoint.h"
#include "game/teleport.h"
#include "game/obj_pool.h"

int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IOLBF,0);
    if((argc!=3 && argc!=4) || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
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
    GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;
    if(!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game))return 4;
    if(!gameuilib_init(&game) || !gamelib_mod_load("Arcanum") || !gameuilib_mod_load())return 5;
    gamelib_splash_clear();
    if(!modern_ui_init() || !gamelib_load(argv[2]))return 6;
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(!sub_45B300());

    if(argc==4) { assert(!strcmp(argv[3],"fresh")); assert(remove("modules/Arcanum/Save/Current/maps/Arcanum1-024-fixed/68853695431.dif")==0); }
    if(map_current_map()==12) {
        JumpPoint out; assert(jumppoint_get(location_make(109,92),&out));
        TeleportData jump={.obj=player_get_local_pc_obj(),.map=out.dst_map,.loc=out.dst_loc};
        printf("EXIT to %d\n",out.dst_map);
        assert(teleport_do(&jump));teleport_ping(0);
    }
    int64_t pc=player_get_local_pc_obj();
    object_flags_unset(pc,OF_OFF);
    int64_t loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
    location_origin_set(loc);
    printf("MAP %d LOC %d,%d\n",map_current_map(),(int)LOCATION_GET_X(loc),(int)LOCATION_GET_Y(loc));
    int iter; int64_t obj;
    Sector* affected;
    assert(sector_lock(68853695431LL, &affected));
    assert(sector_unlock(68853695431LL));
    int64_t guard=OBJ_HANDLE_NULL;
    int guard_frames=0; tig_art_id_t previous_guard_art=0;
    int objects_before=0;
    if(obj_pool_walk_first(&obj,&iter))do { objects_before++; }while(obj_pool_walk_next(&obj,&iter));
    if(obj_pool_walk_first(&obj,&iter)) do {
        int type=obj_field_int32_get(obj,OBJ_F_TYPE);
        if(type!=OBJ_TYPE_NPC && type!=OBJ_TYPE_PC)continue;
        int64_t pos=obj_field_int64_get(obj,OBJ_F_LOCATION);
        if(llabs(LOCATION_GET_X(pos)-LOCATION_GET_X(loc))>30 || llabs(LOCATION_GET_Y(pos)-LOCATION_GET_Y(loc))>30)continue;
        if(type==OBJ_TYPE_NPC && LOCATION_GET_X(pos)==61977 && LOCATION_GET_Y(pos)==65660)guard=obj;
        tig_art_id_t aid=obj_field_int32_get(obj,OBJ_F_CURRENT_AID);
        char path[260]={0}; name_resolve_path(aid,path,sizeof(path));
        TigArtFrameData frame;
        Uint64 start=SDL_GetTicks(); int rc=tig_art_frame_data(aid,&frame);
        printf("ART %08x %s rc=%d ms=%llu pos=%d,%d\n",aid,path,rc,SDL_GetTicks()-start,(int)LOCATION_GET_X(pos),(int)LOCATION_GET_Y(pos));
    } while(obj_pool_walk_next(&obj,&iter));
    assert(guard!=OBJ_HANDLE_NULL);
    ObjectID guard_id=obj_get_id(guard);
    for(int i=0;i<180;i++) {
        Uint64 a=SDL_GetTicks();tig_ping();gamelib_ping();Uint64 b=SDL_GetTicks();
        iso_invalidate_rect(NULL);iso_redraw();Uint64 c=SDL_GetTicks();tig_window_display();Uint64 d=SDL_GetTicks();
        assert(obj_pool_perm_lookup(guard_id)==guard);
        tig_art_id_t guard_art=obj_field_int32_get(guard,OBJ_F_CURRENT_AID);
        TigArtFrameData frame;assert(tig_art_frame_data(guard_art,&frame)==TIG_OK);
        assert(frame.width>0 && frame.height>0);
        if(guard_art!=previous_guard_art){guard_frames++;previous_guard_art=guard_art;}
        if(i%30==0)printf("FRAME %d update=%llu draw=%llu present=%llu\n",i,b-a,c-b,d-c);
        TigMessage msg;while(tig_message_dequeue(&msg)==TIG_OK){}
    }
    int objects_after=0;
    if(obj_pool_walk_first(&obj,&iter))do { objects_after++; }while(obj_pool_walk_next(&obj,&iter));
    printf("OBJECTS before=%d after=%d GUARD animation changes=%d\n",objects_before,objects_after,guard_frames);
    assert(objects_after<=objects_before+10 && guard_frames>1);
    ObjectID saved_ids[1024]; int saved_fields[1024], saved_values[1024], saved_count=0;
    if(obj_pool_walk_first(&obj,&iter))do {
        if(!object_is_static(obj) || obj_get_id(obj).type==OID_TYPE_NULL
            || sector_id_from_loc(obj_field_int64_get(obj,OBJ_F_LOCATION))!=68853695431LL)continue;
        int type=obj_field_int32_get(obj,OBJ_F_TYPE);
        int field=type==OBJ_TYPE_PORTAL ? OBJ_F_PORTAL_FLAGS : type==OBJ_TYPE_CONTAINER ? OBJ_F_CONTAINER_FLAGS : 0;
        if(!field)continue;
        assert(saved_count<1024);
        saved_ids[saved_count]=obj_get_id(obj);saved_fields[saved_count]=field;
        saved_values[saved_count++]=obj_field_int32_get(obj,field);
    }while(obj_pool_walk_next(&obj,&iter));
    assert(saved_count>0);
    assert(gamelib_save("Slot0093","UAP sector compatibility test"));
    assert(gamelib_load("Slot0093"));
    assert(sector_lock(68853695431LL,&affected));assert(sector_unlock(68853695431LL));
    guard=obj_pool_perm_lookup(guard_id);assert(obj_handle_is_valid(guard));
    for(int i=0;i<saved_count;i++) {
        int64_t restored=obj_pool_perm_lookup(saved_ids[i]);assert(obj_handle_is_valid(restored));
        assert(obj_field_int32_get(restored,saved_fields[i])==saved_values[i]);
    }
    printf("PASS: %d saved portal/container states preserved\n",saved_count);
    puts("PASS: mansion exit, saved sector, stable objects, animated guard and save/load");
    return 0;
}
