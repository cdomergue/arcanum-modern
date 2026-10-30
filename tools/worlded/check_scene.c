/* Opt-in regression against a generated playtest, never the source game.
 * Usage: check-worlded-scene COPY/game SLOT X Y [localX localY]
 * Verifies case-insensitive mobile loading and static DIF order across reloads.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
#include "game/sector_object_list.h"
#include "game/map.h"
#include "game/teleport.h"
#include "game/obj_pool.h"
#include "game/item.h"
#include "game/portal.h"
#include "game/path.h"
#include "ui/iso.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"Failed: %s (line %d)\n",#x,__LINE__);return 1;}} while(0)
static void render(void) {
    for(int i=0;i<3;i++) {modern_ui_tick();tig_ping();gamelib_ping();iso_invalidate_rect(NULL);iso_redraw();tig_window_display();TigMessage msg;while(tig_message_dequeue(&msg)==TIG_OK){}}
}
int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if((argc!=5 && argc!=7) || chdir(argv[1]) || access(".worlded-playtest",R_OK)) return 2;
    int sx=atoi(argv[3]),sy=atoi(argv[4]);if(sx<0 || sx>=2000 || sy<0 || sy>=2000)return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    puts("Initialize TIG");
    highres_config_load();TigInitInfo init={0};init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    REQUIRE(tig_init(&init)==TIG_OK);puts("Initialize game");intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;
    REQUIRE(intgame_create_iso_window(&game.iso_window_handle));REQUIRE(gamelib_init(&game));puts("Initialize UI");REQUIRE(gameuilib_init(&game));puts("Load module");
    REQUIRE(gamelib_mod_load("Arcanum"));REQUIRE(gameuilib_mod_load());gamelib_splash_clear();REQUIRE(modern_ui_init());
    puts("Load copied save");REQUIRE(gamelib_load(argv[2]));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    int world=(map_list_info_find("Arcanum1-024-fixed") + 1);REQUIRE(world>0);
    int64_t pc=player_get_local_pc_obj();ObjectID player_id=obj_get_id(pc);
    int original_map=map_current_map();int64_t original_loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
    int64_t sector=(int64_t)sx | ((int64_t)sy<<26),arrival=location_make(sx*64+(argc==7?atoi(argv[5]):32),sy*64+(argc==7?atoi(argv[6]):32));
    printf("Loaded copied save, map %d, position %d,%d\n",original_map,(int)LOCATION_GET_X(original_loc),(int)LOCATION_GET_Y(original_loc));
    TeleportData jump={.obj=pc,.map=world,.loc=arrival};REQUIRE(teleport_do(&jump));teleport_ping(0);
    pc=player_get_local_pc_obj();REQUIRE(obj_pool_perm_lookup(player_id)==pc);object_flags_unset(pc,OF_OFF);location_origin_set(arrival);
    Sector* loaded;REQUIRE(sector_lock(sector,&loaded));REQUIRE(sector_unlock(sector));
    // Linux must not resolve differently depending on the spelling used by
    // maplist.mes, a save, or the editor. Read both through TIG itself.
    const char* paths[]={"maps/qrmvs-420-1zjcnpgN","maps/qrmvs-420-1zjcnpgn"};
    TigFile* a=tig_file_fopen(paths[0],"rb"),*b=tig_file_fopen(paths[1],"rb");REQUIRE(a && b);
    REQUIRE(tig_file_filelength(a)==tig_file_filelength(b));int ca,cb;do{ca=tig_file_fgetc(a);cb=tig_file_fgetc(b);REQUIRE(ca==cb);}while(ca!=EOF);tig_file_fclose(a);tig_file_fclose(b);
    struct Snapshot {int64_t loc;int type;unsigned aid;} snapshots[10000];int static_count=0,mobile_count=0,npc_count=0;
    for(int pass=0;pass<3;pass++) {
        REQUIRE(sector_lock(sector,&loaded));int ordinal=0;
        for(int t=0;t<4096;t++)for(ObjectNode* n=loaded->objects.heads[t];n;n=n->next)if(object_is_static(n->obj)){
            int temp=obj_field_int32_get(n->obj,OBJ_F_TEMP_ID);REQUIRE(temp==ordinal && ordinal<10000);
            struct Snapshot now={obj_field_int64_get(n->obj,OBJ_F_LOCATION),obj_field_int32_get(n->obj,OBJ_F_TYPE),obj_field_int32_get(n->obj,OBJ_F_AID)};
            if(pass==0)snapshots[ordinal]=now;else REQUIRE(now.loc==snapshots[ordinal].loc && now.type==snapshots[ordinal].type && now.aid==snapshots[ordinal].aid);
            ordinal++;
        }
        sector_unlock(sector);if(pass==0)static_count=ordinal;else REQUIRE(ordinal==static_count);

        int iter,counts[18]={0};int64_t obj;
        if(obj_pool_walk_first(&obj,&iter)) do {
            if(obj_is_proto(obj) || sector_id_from_loc(obj_field_int64_get(obj,OBJ_F_LOCATION))!=sector)continue;
            int type=obj_field_int32_get(obj,OBJ_F_TYPE);counts[type]++;
            if(type==2 || type==16 || type==1) {
                int64_t loc=obj_field_int64_get(obj,OBJ_F_LOCATION);int flags=obj_field_int32_get(obj,OBJ_F_FLAGS);
                int tile=-1;REQUIRE(sector_lock(sector,&loaded));bool in_sector=false;for(int t=0;t<4096;t++)for(ObjectNode* n=loaded->objects.heads[t];n;n=n->next)if(n->obj==obj){in_sector=true;tile=t;}sector_unlock(sector);REQUIRE(in_sector);(void)tile;
                printf("OBJECT pass=%d type=%d loc=%d,%d flags=%x aid=%x\n",pass,type,(int)LOCATION_GET_X(loc)%64,(int)LOCATION_GET_Y(loc)%64,flags,obj_field_int32_get(obj,OBJ_F_CURRENT_AID));
            }
        }while(obj_pool_walk_next(&obj,&iter));
        printf("TOTAL pass=%d walls=%d doors=%d containers=%d npc=%d\n",pass,counts[0],counts[1],counts[2],counts[16]);
        // Optional regression for a project NPC whose source includes armor
        // and a weapon; verify equipment survives reload and sector eviction.
        const char* expected_source=getenv("WORLDED_EXPECT_EQUIPPED_SOURCE");
        if(expected_source){int actors=0,walk;int64_t npc;
            if(obj_pool_walk_first(&npc,&walk))do{if(!obj_is_proto(npc) && obj_field_int32_get(npc,OBJ_F_TYPE)==OBJ_TYPE_NPC && item_inventory_source(npc)==atoi(expected_source) && sector_id_from_loc(obj_field_int64_get(npc,OBJ_F_LOCATION))==sector){
                REQUIRE(item_wield_get(npc,ITEM_INV_LOC_ARMOR)!=OBJ_HANDLE_NULL);REQUIRE(item_wield_get(npc,ITEM_INV_LOC_WEAPON)!=OBJ_HANDLE_NULL);
                REQUIRE(tig_art_critter_id_armor_get(obj_field_int32_get(npc,OBJ_F_CURRENT_AID))!=TIG_ART_ARMOR_TYPE_UNDERWEAR);actors++;
            }}while(obj_pool_walk_next(&npc,&walk));REQUIRE(actors>0);
        }
        if(pass==0){mobile_count=counts[2];npc_count=counts[16];}else REQUIRE(counts[2]==mobile_count && counts[16]==npc_count);
        location_origin_set(arrival);render();REQUIRE(tig_video_screenshot_make()==TIG_OK);
        if(pass==0){REQUIRE(gamelib_save("Slot0098","WorldED regression"));REQUIRE(gamelib_load("Slot0098"));}
        if(pass==1){
            jump=(TeleportData){.obj=player_get_local_pc_obj(),.map=world,.loc=location_make(970*64+30,1025*64+30)};REQUIRE(teleport_do(&jump));teleport_ping(0);
            for(int y=1018;y<1030;y++)for(int x=960;x<980;x++){int64_t sid=x|((int64_t)y<<26);if(sector_lock(sid,&loaded))sector_unlock(sid);}
            jump=(TeleportData){.obj=player_get_local_pc_obj(),.map=world,.loc=arrival};REQUIRE(teleport_do(&jump));teleport_ping(0);
        }
    }
    puts("PASS: both mobile bundle spellings, populated sectors, static order/positions/art after save/load and sector eviction");
    return 0;
}
