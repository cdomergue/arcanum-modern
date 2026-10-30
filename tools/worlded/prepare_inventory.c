/* Native inventory materialization for WorldED copies.
 * Usage: worlded-native-inventory COPY/game SLOT REQUESTS OUTPUT_DIRECTORY SECTOR_X SECTOR_Y
 * Refuses source installations; never saves or replaces a campaign slot. */
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
#include "game/script.h"
#include "game/script_name.h"
#include "game/dialog.h"
#include "game/obj_file.h"
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
    if(argc!=7 || chdir(argv[1]) || access(".worlded-playtest",R_OK)) return 2;
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
    int sx=atoi(argv[5]),sy=atoi(argv[6]);REQUIRE(sx>=0 && sx<2000 && sy>=0 && sy<2000);
    TeleportData jump={.obj=player_get_local_pc_obj(),.map=map_list_info_find("Arcanum1-024-fixed")+1,.loc=location_make(sx*64+32,sy*64+32)};
    REQUIRE(jump.map>0 && teleport_do(&jump));teleport_ping(0);
    FILE* requests=fopen(argv[3],"r");REQUIRE(requests!=NULL);
    char wanted[40];int prepared=0;
    while(fscanf(requests,"%39s",wanted)==1){
        REQUIRE(strlen(wanted)==36);ObjectID id={0};unsigned char* raw=(unsigned char*)&id;
        for(int i=0;i<18;i++){unsigned n;REQUIRE(sscanf(wanted+i*2,"%2x",&n)==1);raw[i<2?i:i+6]=(unsigned char)n;}
        int64_t npc=obj_pool_perm_lookup(id);REQUIRE(npc!=OBJ_HANDLE_NULL);REQUIRE(obj_field_int32_get(npc,OBJ_F_TYPE)==OBJ_TYPE_NPC);
        if(item_inventory_source(npc)==0)continue;
        int count=obj_field_int32_get(npc,OBJ_F_CRITTER_INVENTORY_NUM);REQUIRE(count>=0 && count<10000);
        bool has_items=false;for(int i=0;i<count;i++)if(obj_arrayfield_handle_get(npc,OBJ_F_CRITTER_INVENTORY_LIST_IDX,i)!=OBJ_HANDLE_NULL){has_items=true;break;}
        if(has_items)continue; // Existing inventory belongs to the author; never replace it.
        obj_field_int32_set(npc,OBJ_F_CRITTER_INVENTORY_NUM,0);
        sub_463C60(npc);item_wield_best_all(npc,OBJ_HANDLE_NULL);
        count=obj_field_int32_get(npc,OBJ_F_CRITTER_INVENTORY_NUM);
        REQUIRE(count>=0 && count<10000);int64_t* items=malloc(sizeof(int64_t)*(count+1));REQUIRE(items!=NULL);items[0]=npc;
        for(int i=0;i<count;i++){items[i+1]=obj_arrayfield_handle_get(npc,OBJ_F_CRITTER_INVENTORY_LIST_IDX,i);REQUIRE(items[i+1]!=OBJ_HANDLE_NULL);obj_get_id(items[i+1]);}
        printf("Prepared NPC %s: %d items, armor %lld, weapon %lld\n",wanted,count,(long long)item_wield_get(npc,ITEM_INV_LOC_ARMOR),(long long)item_wield_get(npc,ITEM_INV_LOC_WEAPON));
        for(int i=0;i<=count;i++)obj_save_preprocess(items[i]);
        for(int i=0;i<=count;i++)REQUIRE(objf_solitary_write(items[i],argv[4],".mob"));
        for(int i=0;i<=count;i++)obj_load_postprocess(items[i]);free(items);prepared++;

    }
    fclose(requests);printf("Prepared inventories: %d\n",prepared);
    gameuilib_exit();gamelib_exit();tig_exit();return 0;
}
