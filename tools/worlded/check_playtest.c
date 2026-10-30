/* Exercises a copied save through the actual CE engine. No original game paths
 * are writable: the target must be a marked WorldED playtest installation. */
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
    if((argc!=3 && !(argc==4 && !strcmp(argv[3],"--reload"))) || chdir(argv[1]) || access(".worlded-playtest",R_OK)) return 2;
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
    int64_t sector=1346LL | (1024LL<<26),arrival=location_make(1346*64+32,1024*64+32);
    printf("Loaded copied save, map %d, position %d,%d\n",original_map,(int)LOCATION_GET_X(original_loc),(int)LOCATION_GET_Y(original_loc));
    TeleportData jump={.obj=pc,.map=world,.loc=arrival};REQUIRE(teleport_do(&jump));teleport_ping(0);
    pc=player_get_local_pc_obj();REQUIRE(obj_pool_perm_lookup(player_id)==pc);object_flags_unset(pc,OF_OFF);location_origin_set(arrival);
    Sector* loaded;REQUIRE(sector_lock(sector,&loaded));REQUIRE(sector_unlock(sector));
    int64_t chest[4]={0},door=0,obj;int iter,containers=0,doors=0,walls=0;
    if(obj_pool_walk_first(&obj,&iter)) do {
        if(obj_is_proto(obj) || sector_id_from_loc(obj_field_int64_get(obj,OBJ_F_LOCATION))!=sector) continue;
        int type=obj_field_int32_get(obj,OBJ_F_TYPE);
        if(type==OBJ_TYPE_CONTAINER) {REQUIRE(containers<4);chest[containers++]=obj;}
        if(type==OBJ_TYPE_PORTAL) {door=obj;doors++;}
        if(type==OBJ_TYPE_WALL)walls++;
        tig_art_id_t aid=obj_field_int32_get(obj,OBJ_F_CURRENT_AID);TigArtFrameData frame;REQUIRE(tig_art_frame_data(aid,&frame)==TIG_OK);
    } while(obj_pool_walk_next(&obj,&iter));
    printf("Loaded target sector: %d walls, %d doors, %d containers\n",walls,doors,containers);
    REQUIRE(containers==4 && doors==1 && walls==44);
    if(argc==4) {int items=0;for(int i=0;i<4;i++){int64_t item=item_find_first(chest[i]);if(item!=OBJ_HANDLE_NULL){REQUIRE(obj_field_int32_get(item,OBJ_F_TYPE)==OBJ_TYPE_GOLD);REQUIRE(obj_field_int32_get(item,OBJ_F_GOLD_QUANTITY)==1);items++;}}
        REQUIRE(items==1);puts("PASS: stored object survives full process restart");return 0;}

    REQUIRE(!(obj_field_int32_get(door,OBJ_F_PORTAL_FLAGS)&1));
    for(int i=0;i<4;i++){REQUIRE(!(obj_field_int32_get(chest[i],OBJ_F_CONTAINER_FLAGS)&1));REQUIRE(item_inventory_source(chest[i])==0);REQUIRE(item_find_first(chest[i])==OBJ_HANDLE_NULL);}
    REQUIRE(!portal_is_open(door));for(int i=0;i<12;i++)portal_open(door);REQUIRE(portal_is_open(door));
    int8_t steps[256];PathCreateInfo path={.obj=pc,.from=arrival,.to=location_make(1346*64+30,1024*64+25),.max_rotations=256,.rotations=steps};
    int walking=path_make(&path);printf("Walk from arrival through doorway: %d steps\n",walking);REQUIRE(walking>0);
    printf("PC location before render: %lld expected %lld\n",(long long)obj_field_int64_get(pc,OBJ_F_LOCATION),(long long)arrival);
    REQUIRE(obj_field_int64_get(pc,OBJ_F_LOCATION)==arrival);
    render();REQUIRE(tig_video_screenshot_make()==TIG_OK);
    REQUIRE(obj_field_int64_get(pc,OBJ_F_LOCATION)==arrival);
    for(int i=0;i<12;i++)portal_close(door);REQUIRE(!portal_is_open(door));
    WmapPathInfo route={.from=1230LL|(980LL<<26),.to=sector,.max_rotations=256,.rotations=steps};
    int travel=wmap_path_make(&route);printf("World travel route: %d sectors\n",travel);REQUIRE(travel>0);
    wmap_ui_open();REQUIRE(wmap_ui_is_created());ModernMapLabel labels[200];int count=modern_map_travel_labels(labels,200),matches=0;
    printf("Map labels: %d\n",count); for(int i=0;i<count;i++) printf("LABEL %d,%d %s\n",labels[i].x,labels[i].y,labels[i].text);
    for(int i=0;i<count;i++) if(labels[i].x==654 && labels[i].y==1024) {printf("World marker: %s\n",labels[i].text);matches++;}
    REQUIRE(matches==1);render();REQUIRE(tig_video_screenshot_make()==TIG_OK);wmap_ui_close();
    wmap_ui_open();count=modern_map_travel_labels(labels,200);matches=0;for(int i=0;i<count;i++) if(labels[i].x==654 && labels[i].y==1024) matches++;REQUIRE(matches==1);wmap_ui_close();
    ObjectID chest_id=obj_get_id(chest[0]);int64_t token=item_gold_create(1,arrival);REQUIRE(token!=OBJ_HANDLE_NULL);REQUIRE(item_transfer(token,chest[0]));
    ObjectID token_id=obj_get_id(token); REQUIRE(item_find_first(chest[0])==token); REQUIRE(obj_field_int32_get(token,OBJ_F_GOLD_QUANTITY)==1);
    REQUIRE(gamelib_save("Slot0098","WorldED - test stockage"));REQUIRE(gamelib_load("Slot0098"));
    pc=player_get_local_pc_obj();REQUIRE(obj_pool_perm_lookup(player_id)==pc);int64_t restored=obj_pool_perm_lookup(chest_id);REQUIRE(restored!=OBJ_HANDLE_NULL);REQUIRE(item_find_first(restored)==obj_pool_perm_lookup(token_id));REQUIRE(obj_field_int32_get(item_find_first(restored),OBJ_F_GOLD_QUANTITY)==1);
    jump=(TeleportData){.obj=pc,.map=original_map,.loc=original_loc};REQUIRE(teleport_do(&jump));teleport_ping(0);
    jump=(TeleportData){.obj=player_get_local_pc_obj(),.map=world,.loc=arrival};REQUIRE(teleport_do(&jump));teleport_ping(0);
    restored=obj_pool_perm_lookup(chest_id);REQUIRE(restored!=OBJ_HANDLE_NULL);REQUIRE(item_find_first(restored)==obj_pool_perm_lookup(token_id));REQUIRE(obj_field_int32_get(item_find_first(restored),OBJ_F_GOLD_QUANTITY)==1);
    // Leave the selected source slot untouched even inside the copy. The test
    // slot documents validation; editor launch always starts from Slot0002.
    object_flags_unset(player_get_local_pc_obj(),OF_OFF);location_origin_set(arrival);render();REQUIRE(tig_video_screenshot_make()==TIG_OK);
    puts("PASS: copied character, house assets, unlocked door/chests, walkable entrance, world route/marker, storage after save/load and return");
    return 0;
}
