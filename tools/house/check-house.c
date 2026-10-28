/* Native housing regression: check-house COPY/game SLOT.
 * This harness refuses any directory without the test-copy marker. */
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
#include "game/modern_house.h"
#include "game/mes.h"
#include "game/obj_flags.h"
#include "game/script_name.h"
#include "game/dialog.h"
#include "ui/iso.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include "ui/dialog_ui.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"Failed: %s (line %d)\n",#x,__LINE__);return 1;}} while(0)
static void render(void) {
    for(int i=0;i<3;i++) {modern_ui_tick();tig_ping();gamelib_ping();iso_invalidate_rect(NULL);iso_redraw();tig_window_display();TigMessage msg;while(tig_message_dequeue(&msg)==TIG_OK){}}
}
static void capture(const char* label) {
    render();SDL_Renderer* renderer=NULL;
    if(tig_video_renderer_get(&renderer)!=TIG_OK || !renderer)return;
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);
    if(surface){char path[200];snprintf(path,sizeof(path),"/tmp/house-%s-%s.png",getenv("ARCANUM_HOUSE_LANGUAGE")?getenv("ARCANUM_HOUSE_LANGUAGE"):"en",label);SDL_SavePNG(surface,path);SDL_DestroySurface(surface);}
}
static int check_vegetation(bool inject)
{
    mes_file_handle_t rules;REQUIRE(mes_load("house/scene/vegetation.mes",&rules));
    int64_t plant=0,obj;int iter;
    if(obj_inst_first(&obj,&iter))do{
        if(obj_field_int32_get(obj,OBJ_F_TYPE)==OBJ_TYPE_SCENERY
          && tig_art_scenery_id_type_get(obj_field_int32_get(obj,OBJ_F_CURRENT_AID))==TIG_ART_SCENERY_TYPE_PLANTS){plant=obj;break;}
    }while(obj_inst_next(&obj,&iter));
    REQUIRE(plant);
    if(inject){
        /* Exercise a saved dynamic variant, not just the original static art. */
        int tiles[]={20*64+25,10*64+15};
        for(int i=0;i<2;i++){
            MesFileEntry e={.num=tiles[i]};REQUIRE(mes_search(rules,&e));REQUIRE(strtoul(e.str,NULL,10)&32);
            int64_t copy;REQUIRE(object_duplicate(plant,location_make(1346*64+tiles[i]%64,1024*64+tiles[i]/64),&copy));
            object_flags_unset(copy,OF_OFF|OF_DONTDRAW);
        }
        int64_t gold=item_gold_create(19,location_make(1346*64+25,1024*64+20));REQUIRE(gold);
        gamelib_ping_time+=1000;modern_house_tick();
        REQUIRE(!(obj_field_int32_get(gold,OBJ_F_FLAGS)&OF_OFF));
    }
    int checked=0;
    if(obj_inst_first(&obj,&iter))do{
        if(obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_SCENERY)continue;
        int64_t loc=obj_field_int64_get(obj,OBJ_F_LOCATION);
        if(sector_id_from_loc(loc)!=SECTOR_MAKE(1346LL,1024LL))continue;
        MesFileEntry e={.num=(LOCATION_GET_Y(loc)&63)*64+(LOCATION_GET_X(loc)&63)};
        if(!mes_search(rules,&e))continue;
        int kind=tig_art_scenery_id_type_get(obj_field_int32_get(obj,OBJ_F_CURRENT_AID));
        if(strtoul(e.str,NULL,10)&(1u<<kind)){REQUIRE(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_OFF);checked++;}
    }while(obj_inst_next(&obj,&iter));
    REQUIRE(checked>0);mes_unload(rules);
    printf("PASS: %d vegetation objects hidden in clearance tiles; items preserved\n",checked);return 0;
}
int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if((argc!=3 && argc!=4) || chdir(argv[1]) || access(".renaissance-house-test",R_OK)) return 2;
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

    if(argc==4 && !strcmp(argv[3],"--check-agent")){
        int64_t agent=0,previous=0;ObjectID expected={0};
        for(int pass=0;pass<4;pass++){
            gamelib_ping_time+=1000;modern_house_tick();
            int count=0,iter;int64_t obj;
            if(obj_inst_first(&obj,&iter))do{
                if(obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_NPC || obj_field_int32_get(obj,OBJ_F_DESCRIPTION)!=61002)continue;
                if(obj_field_int32_get(obj,OBJ_F_FLAGS)&(OF_OFF|OF_DONTDRAW|OF_DESTROYED))continue;
                agent=obj;count++;
            }while(obj_inst_next(&obj,&iter));
            REQUIRE(count==1);REQUIRE(obj_field_int32_get(agent,OBJ_F_CRITTER_DESCRIPTION_UNKNOWN)==61005);
            REQUIRE(obj_field_int64_get(agent,OBJ_F_LOCATION)==location_make(61991,65620));
            if(pass==0){expected=obj_get_id(agent);previous=agent;}
            else REQUIRE(objid_is_equal(expected,obj_get_id(agent)));
            int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
            object_script_execute(pc,agent,0,SAP_DIALOG,0);REQUIRE(dialog_ui_is_local_pc_in_dialog());dialog_ui_end_dialog(pc,0);
            if(pass==0){
                int64_t duplicate;REQUIRE(object_duplicate(agent,obj_field_int64_get(agent,OBJ_F_LOCATION),&duplicate));
                REQUIRE(duplicate!=previous);puts("Injected a legacy duplicate to test save repair");
            }
            REQUIRE(gamelib_save("Slot0095","Agent regression"));REQUIRE(gamelib_load("Slot0095"));
        }
        sub_43E770(player_get_local_pc_obj(),location_make(61994,65624),0,0);
        location_origin_set(location_make(61991,65620));capture("agent-placement");
        puts("PASS: one active agent after four save/reloads, legacy duplicate retired, identity stable and dialogue opens");
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }
    if(argc==4 && !strcmp(argv[3],"--inspect-agent")){
        int64_t pc=player_get_local_pc_obj(),pc_loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
        printf("PLAYER %d %d offsets=%d,%d rotation=%d\n",(int)LOCATION_GET_X(pc_loc),(int)LOCATION_GET_Y(pc_loc),obj_field_int32_get(pc,OBJ_F_OFFSET_X),obj_field_int32_get(pc,OBJ_F_OFFSET_Y),tig_art_id_rotation_get(obj_field_int32_get(pc,OBJ_F_CURRENT_AID)));
        for(int pass=0;pass<2;pass++){
            int64_t o;int iter;
            if(pass){gamelib_ping_time+=1000;modern_house_tick();}
            if(obj_inst_first(&o,&iter))do{
                if(obj_field_int32_get(o,OBJ_F_TYPE)!=OBJ_TYPE_NPC)continue;
                int64_t loc=obj_field_int64_get(o,OBJ_F_LOCATION);
                int x=LOCATION_GET_X(loc),y=LOCATION_GET_Y(loc),desc=obj_field_int32_get(o,OBJ_F_DESCRIPTION);
                if(desc==61002 || (abs(x-61997)<15 && abs(y-65626)<15)){
                    char id[100];objid_id_to_str(id,obj_get_id(o));Script scr={0};obj_arrayfield_script_get(o,OBJ_F_SCRIPTS_IDX,SAP_DIALOG,&scr);
                    printf("ACTOR pass=%d handle=%lld id=%s loc=%d,%d desc=%d unknown=%d script=%d flags=%x\n",pass,(long long)o,id,x,y,desc,obj_field_int32_get(o,OBJ_F_CRITTER_DESCRIPTION_UNKNOWN),scr.num,obj_field_int32_get(o,OBJ_F_FLAGS));
                }
            }while(obj_inst_next(&o,&iter));
        }
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }
    if(argc==4 && !strcmp(argv[3],"--verify-start")){
        int64_t pc=player_get_local_pc_obj(),loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
        REQUIRE(script_global_var_get(1989)==0 && !sub_463370(pc,61003));
        if(!strcmp(argv[2],"Slot0096"))REQUIRE(sector_id_from_loc(loc)==SECTOR_MAKE(1346LL,1024LL));
        else if(!strcmp(argv[2],"Slot0097")){REQUIRE(loc==location_make(61996,65628));REQUIRE(item_gold_get(pc)==10000);}
        else REQUIRE(0);
        printf("PASS: %s reloads unowned, no key, location %d,%d, coins %d\n",argv[2],(int)LOCATION_GET_X(loc),(int)LOCATION_GET_Y(loc),item_gold_get(pc));
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }
    char path[260];int dlg;
    REQUIRE(script_name_build_dlg_name(48301,path,sizeof(path)));REQUIRE(dialog_load(path,&dlg));dialog_unload(dlg);
    REQUIRE(modern_house_description(61003)!=NULL);
    int world=map_list_info_find("Arcanum1-024-fixed")+1;
    int64_t pc=player_get_local_pc_obj();
    TeleportData jump={.obj=pc,.map=world,.loc=location_make(1346*64+30,1024*64+30)};
    REQUIRE(teleport_do(&jump));teleport_ping(0);
    gamelib_ping_time+=1000;modern_house_tick();
    mes_file_handle_t manifest;REQUIRE(mes_load("house/scene/scene.mes",&manifest));
    MesFileEntry e;int count=0;int64_t guard=0,chest=0;
    for(bool more=mes_find_first(manifest,&e);more;more=mes_find_next(manifest,&e)){
        if(e.num<100)continue;char role[20],idstr[40];REQUIRE(sscanf(e.str,"%19s %39s",role,idstr)==2);
        ObjectID id={0};unsigned char* raw=(unsigned char*)&id;
        for(int i=0;i<18;i++){unsigned n;REQUIRE(sscanf(idstr+2*i,"%2x",&n)==1);raw[i<2?i:i+6]=n;}
        int64_t o=obj_pool_perm_lookup(id);REQUIRE(o!=0);count++;
        if(!strcmp(role,"guard"))guard=o;
        if(!strcmp(role,"chest"))chest=o;
        if(!strcmp(role,"portal"))REQUIRE((obj_field_int32_get(o,OBJ_F_PORTAL_FLAGS)&0x15)==0x15);
    }
    REQUIRE(check_vegetation(true)==0);
    REQUIRE(count==115 && guard && chest);puts("PASS: existing save migrated, 115 authored objects, all windows and doors locked");
    REQUIRE(obj_field_int32_get(guard,OBJ_F_CRITTER_INVENTORY_NUM)>=6);
    REQUIRE(script_global_var_get(1989)==0 && !sub_463370(pc,61003));
    REQUIRE(gamelib_save("Slot0096","Maison - devant le garde"));
    int64_t stored=item_gold_create(77,obj_field_int64_get(chest,OBJ_F_LOCATION));REQUIRE(stored && item_transfer(stored,chest));
    ObjectID chest_id=obj_get_id(chest),stored_id=obj_get_id(stored);
    object_script_execute(pc,guard,0,SAP_DIALOG,0);REQUIRE(dialog_ui_is_local_pc_in_dialog());capture("guard");dialog_ui_end_dialog(pc,0);
    capture("house");
    int gold=item_gold_get(pc);REQUIRE(item_gold_transfer(pc,0,gold,0));REQUIRE(item_gold_transfer(0,pc,9999,0));
    jump.obj=pc;jump.loc=location_make(61996,65628);REQUIRE(teleport_do(&jump));teleport_ping(0);
    gamelib_ping_time+=1000;modern_house_tick();
    int64_t agent=0,o;int iter;
    if(obj_inst_first(&o,&iter))do{if(obj_field_int32_get(o,OBJ_F_DESCRIPTION)==61002)agent=o;}while(obj_inst_next(&o,&iter));
    REQUIRE(agent);printf("Agent at %lld\n",(long long)obj_field_int64_get(agent,OBJ_F_LOCATION));
    object_flags_unset(pc,OF_OFF);location_origin_set(jump.loc);
    const int nodes[]={1,1000,3000,3010};const char* labels[]={"greeting","catalogue","details","confirm"};
    for(int k=0;k<4;k++){
        dialog_ui_start_dialog(pc,agent,48301,0,nodes[k]);REQUIRE(dialog_ui_is_local_pc_in_dialog());capture(labels[k]);dialog_ui_end_dialog(pc,0);
    }
    Script script={.num=48301};ScriptInvocation inv={.script=&script,.line=101,.triggerer_obj=pc,.attachee_obj=agent,.attachment_point=SAP_DIALOG};bool result;
    REQUIRE(modern_house_script(&inv,&result));REQUIRE(item_gold_get(pc)==9999);REQUIRE(script_global_var_get(1989)==0);puts("PASS: insufficient funds leaves money and ownership unchanged");
    dialog_ui_end_dialog(pc,0);
    REQUIRE(item_gold_transfer(0,pc,1,0));REQUIRE(gamelib_save("Slot0097","Maison - avant achat"));REQUIRE(modern_house_script(&inv,&result));
    REQUIRE(script_global_var_get(1989)==1);REQUIRE(item_gold_get(pc)==0);REQUIRE(sub_463370(pc,61003));puts("PASS: exactly 10000 coins charged; house key issued");
    capture("purchased");dialog_ui_end_dialog(pc,0);
    REQUIRE(item_gold_transfer(0,pc,123,0));REQUIRE(modern_house_script(&inv,&result));REQUIRE(item_gold_get(pc)==123);puts("PASS: repeat purchase cannot charge again");
    dialog_ui_end_dialog(pc,0);
    REQUIRE(gamelib_save("Slot0098","House regression"));REQUIRE(gamelib_load("Slot0098"));
    pc=player_get_local_pc_obj();REQUIRE(script_global_var_get(1989)==1);REQUIRE(sub_463370(pc,61003));REQUIRE(item_gold_get(pc)==123);
    jump.obj=pc;jump.loc=location_make(1346*64+30,1024*64+23);REQUIRE(teleport_do(&jump));teleport_ping(0);
    gamelib_ping_time+=1000;modern_house_tick();
    count=0;int chests=0;
    if(obj_inst_first(&o,&iter))do{if(sector_id_from_loc(obj_field_int64_get(o,OBJ_F_LOCATION))==SECTOR_MAKE(1346LL,1024LL)){
        if(obj_field_int32_get(o,OBJ_F_TYPE)==OBJ_TYPE_CONTAINER){chests++;REQUIRE((obj_field_int32_get(o,OBJ_F_CONTAINER_FLAGS)&0x14)==0);}
    }}while(obj_inst_next(&o,&iter));
    chest=obj_pool_perm_lookup(chest_id);stored=obj_pool_perm_lookup(stored_id);
    REQUIRE(chest && stored);REQUIRE(obj_field_handle_get(stored,OBJ_F_ITEM_PARENT)==chest);REQUIRE(obj_field_int32_get(stored,OBJ_F_GOLD_QUANTITY)==77);
    int x,y;const char* title;REQUIRE(modern_house_location(0,&x,&y,&title));REQUIRE(x==1346 && y==1024);
    location_origin_set(jump.loc);
    REQUIRE(check_vegetation(false)==0);
    capture("return");
    location_origin_set(location_make(1346*64+15,1024*64+10));capture("pond");
    location_origin_set(jump.loc);
    for(int pass=0;pass<2;pass++){
        wmap_ui_open();REQUIRE(wmap_ui_is_created());ModernMapLabel labels[200];int n=modern_map_travel_labels(labels,200),matches=0;
        for(int k=0;k<n;k++)if(labels[k].x==654 && labels[k].y==1024)matches++;
        REQUIRE(matches==1);capture("map");wmap_ui_close();
    }
    puts("PASS: one world-map marker after repeated opening");
    REQUIRE(chests==6);puts("PASS: save/reload/revisit preserves ownership and key, six chests without duplicates");
    gameuilib_exit();gamelib_exit();tig_exit();return 0;
}
