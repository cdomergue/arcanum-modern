/* Woodland House regression fixture: check-house COPY/game SLOT.
 * Coordinates and contents below describe the shipped test fixture only.
 * Property-independent authoring lives in export-scene.cpp and build-content.py.
 * This harness refuses any directory without the test-copy marker. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define chdir _chdir
#define access _access
#define R_OK 4
#else
#include <unistd.h>
#endif
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/critter.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/location.h"
#include "game/light.h"
#include "game/sector.h"
#include "game/sector_object_list.h"
#include "game/sector_light_list.h"
#include "game/map.h"
#include "game/teleport.h"
#include "game/obj_pool.h"
#include "game/item.h"
#include "game/portal.h"
#include "game/path.h"
#include "game/script.h"
#include "game/modern_house.h"
#include "game/modern_world_updates_internal.h"
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
    if(surface){char path[512];const char* dir=getenv("ARCANUM_HOUSE_CAPTURE_DIR");snprintf(path,sizeof(path),"%s/house-%s-%s.png",dir?dir:"/tmp",getenv("ARCANUM_HOUSE_LANGUAGE")?getenv("ARCANUM_HOUSE_LANGUAGE"):"en",label);SDL_SavePNG(surface,path);SDL_DestroySurface(surface);}
}
static int check_hidden_lights(void)
{
    int64_t obj;int iter,checked=0;
    if(obj_inst_first(&obj,&iter))do{
        if(obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_SCENERY
          || !(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_OFF)
          || sector_id_from_loc(obj_field_int64_get(obj,OBJ_F_LOCATION))!=SECTOR_MAKE(1346LL,1024LL))continue;
        if((tig_art_id_t)obj_field_int32_get(obj,OBJ_F_LIGHT_AID)!=TIG_ART_ID_INVALID){
            REQUIRE(obj_field_int32_get(obj,OBJ_F_LIGHT_FLAGS)&LF_OFF);checked++;
        }
        Light* light=obj_field_ptr_get(obj,OBJ_F_LIGHT_HANDLE);
        if(light)REQUIRE(light_get_flags(light)&LF_OFF);
        for(int i=0;i<4;i++){
            if(obj_arrayfield_uint32_get(obj,OBJ_F_OVERLAY_LIGHT_AID,i)!=TIG_ART_ID_INVALID){
                REQUIRE(obj_arrayfield_uint32_get(obj,OBJ_F_OVERLAY_LIGHT_FLAGS,i)&LF_OFF);checked++;
            }
            light=obj_arrayfield_ptr_get(obj,OBJ_F_OVERLAY_LIGHT_HANDLES,i);
            if(light)REQUIRE(light_get_flags(light)&LF_OFF);
        }
    }while(obj_inst_next(&obj,&iter));
    REQUIRE(checked>0);
    printf("PASS: %d hidden scenery light/shade attachments disabled\n",checked);
    return 0;
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

/* A generated terrain sector with saved DIFs must use world tile coordinates.
 * Otherwise invisible scenery can occupy a door tile but evade house clearance. */
static int check_door(void)
{
    int64_t sid=SECTOR_MAKE(1346LL,1024LL),loc=location_make(86175,65562);
    Sector* sec;REQUIRE(sector_lock(sid,&sec));
    for(int tile=0;tile<4096;tile++)for(ObjectNode* n=sec->objects.heads[tile];n;n=n->next){
        REQUIRE(sector_id_from_loc(obj_field_int64_get(n->obj,OBJ_F_LOCATION))==sid);
    }
    sector_unlock(sid);
    gamelib_ping_time+=1000;modern_house_tick();
    int64_t pc=player_get_local_pc_obj(),door=0,obj;int iter;
    ObjectList portals;object_list_location(loc,OBJ_TM_PORTAL,&portals);
    REQUIRE(portals.head && !portals.head->next);door=portals.head->obj;
    object_list_destroy(&portals);
    int actors=0,non_jumpers=0;
    if(obj_inst_first(&obj,&iter))do{
        if(obj!=pc && (obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_NPC || critter_pc_leader_get(obj)!=pc))continue;
        actors++;if(!critter_can_jump_window(obj))non_jumpers++;
        for(int open=0;open<2;open++){
            for(int step=0;step<16 && (open?portal_open(door):portal_close(door));step++){}
            REQUIRE(portal_is_open(door)==(open!=0));
            for(int dir=3;dir<=7;dir+=4){
                int64_t from=dir==3?loc:location_make(86175,65563),to;
                REQUIRE(location_in_dir(from,dir,&to));
                int64_t block=0;int type=-1;bool window=false;
                int cost=object_calc_traversal_cost(obj,from,dir,
                    OBJ_TRAVERSAL_IGNORE_CRITTERS|OBJ_TRAVERSAL_WINDOW_BLOCKS|OBJ_TRAVERSAL_PORTAL_BLOCKS,
                    &block,&type,&window);
                REQUIRE(cost==0 && !window);
                if(!open){REQUIRE(block==door && type==OBJ_TYPE_PORTAL);continue;}
                REQUIRE(block==OBJ_HANDLE_NULL);
                int8_t rotations[8];PathCreateInfo path={.obj=obj,.from=from,.to=to,
                    .max_rotations=8,.rotations=rotations,
                    .flags=PATH_FLAG_0x0002|PATH_FLAG_0x0004|PATH_FLAG_0x0010};
                REQUIRE(path_make(&path)==1 && rotations[0]==dir);
            }
        }
    }while(obj_inst_next(&obj,&iter));
    REQUIRE(actors>0);
    printf("PASS: correct sector coordinates; door blocks closed, direct paths both ways open for %d party members (%d cannot jump windows)\n",actors,non_jumpers);
    return 0;
}

static ObjectID inventory_fixture_id(unsigned index)
{
    ObjectID id={0};unsigned char* raw=(unsigned char*)&id;raw[0]=2;
    for(unsigned n=8;n<24;n++)raw[n]=(unsigned char)(index*16+n-8);
    return id;
}
static int check_independent_light_removal(void)
{
    /* Synthetic nodes are removed before saving: the original SEC/DIF light
     * count must stay unchanged in this copy of an existing campaign save. */
    int64_t sid=SECTOR_MAKE(1346LL,1024LL);Sector* sec;REQUIRE(sector_lock(sid,&sec));
    SectorBlockListNode* previous[4096];int count=0;
    for(SectorBlockListNode* node=sec->lights.head;node;node=node->next){REQUIRE(count<4096);previous[count++]=node;}
    bool was_modified=sec->lights.modified;
    TigFile* stream=tig_file_fopen("house/scene/test-native-lights.bin","rb");REQUIRE(stream);
    Light *target=NULL,*neighbor=NULL,*color_neighbor=NULL,*duplicate=NULL;REQUIRE(light_read(stream,&target));REQUIRE(light_read(stream,&neighbor));REQUIRE(light_read(stream,&color_neighbor));REQUIRE(light_read(stream,&duplicate));tig_file_fclose(stream);
    REQUIRE(target && neighbor && color_neighbor && duplicate && !(target->flags&LF_OFF) && !(neighbor->flags&LF_OFF));
    REQUIRE(sector_light_list_add(&sec->lights,target));REQUIRE(sector_light_list_add(&sec->lights,neighbor));REQUIRE(sector_light_list_add(&sec->lights,color_neighbor));REQUIRE(sector_light_list_add(&sec->lights,duplicate));
    SectorBlockListNode* order[4100];int augmented=0;for(SectorBlockListNode* node=sec->lights.head;node;node=node->next)order[augmented++]=node;
    modern_house_sector_loaded(sid,sec);
    REQUIRE(((target->flags&LF_OFF)!=0) != ((duplicate->flags&LF_OFF)!=0));
    REQUIRE(!(neighbor->flags&LF_OFF) && !(color_neighbor->flags&LF_OFF));
    REQUIRE(target->r==210 && target->g==90 && target->b==30);
    REQUIRE(target->loc==neighbor->loc && target->offset_x==123 && neighbor->offset_x==124 && target->offset_y==-87);
    int seen=0;for(SectorBlockListNode* node=sec->lights.head;node;node=node->next){REQUIRE(seen<augmented && order[seen++]==node);}REQUIRE(seen==count+4);
    unsigned target_flags=target->flags,duplicate_flags=duplicate->flags;
    modern_house_sector_loaded(sid,sec);REQUIRE(target->flags==target_flags && duplicate->flags==duplicate_flags && !(neighbor->flags&LF_OFF) && !(color_neighbor->flags&LF_OFF));
    REQUIRE(sector_light_list_remove(&sec->lights,target));REQUIRE(sector_light_list_remove(&sec->lights,neighbor));REQUIRE(sector_light_list_remove(&sec->lights,color_neighbor));REQUIRE(sector_light_list_remove(&sec->lights,duplicate));sub_4D9570(target);sub_4D9570(neighbor);sub_4D9570(color_neighbor);sub_4D9570(duplicate);sec->lights.modified=was_modified;
    seen=0;for(SectorBlockListNode* node=sec->lights.head;node;node=node->next){REQUIRE(seen<count && previous[seen++]==node);}REQUIRE(seen==count);sector_unlock(sid);
    puts("PASS: independent-light removal matches location/art/offset/color, disables one exact duplicate, leaves neighbors active and preserves native node count/order");return 0;
}
static int check_authored_inventory(void)
{
    int world=map_list_info_find("Arcanum1-024-fixed")+1;
    int64_t pc=player_get_local_pc_obj();int player_gold=item_gold_get(pc);
    TeleportData jump={.obj=pc,.map=world,.loc=location_make(1346*64+30,1024*64+23)};
    REQUIRE(teleport_do(&jump));teleport_ping(0);
    gamelib_ping_time+=1000;modern_house_tick();
    REQUIRE(check_independent_light_removal()==0);
    int64_t chest=obj_pool_perm_lookup(inventory_fixture_id(2));
    int64_t coins=obj_pool_perm_lookup(inventory_fixture_id(3));
    int64_t dagger=obj_pool_perm_lookup(inventory_fixture_id(4));
    int64_t npc=obj_pool_perm_lookup(inventory_fixture_id(5));
    int64_t wielded=obj_pool_perm_lookup(inventory_fixture_id(6));
    REQUIRE(chest && coins && dagger && npc && wielded);
    int64_t proxy=obj_pool_perm_lookup(inventory_fixture_id(7));REQUIRE(proxy);
    Light* authored=obj_field_ptr_get(proxy,OBJ_F_LIGHT_HANDLE);REQUIRE(authored && authored->obj==proxy);
    REQUIRE(authored->r==210 && authored->g==90 && authored->b==30 && !(authored->flags&LF_OFF));
    REQUIRE((obj_field_int32_get(proxy,OBJ_F_FLAGS)&(OF_DONTDRAW|OF_NO_BLOCK))==(OF_DONTDRAW|OF_NO_BLOCK));
    REQUIRE(obj_field_int32_get(chest,OBJ_F_CONTAINER_INVENTORY_NUM)==2);
    REQUIRE(obj_arrayfield_handle_get(chest,OBJ_F_CONTAINER_INVENTORY_LIST_IDX,0)==coins);
    REQUIRE(obj_arrayfield_handle_get(chest,OBJ_F_CONTAINER_INVENTORY_LIST_IDX,1)==dagger);
    REQUIRE(obj_field_handle_get(coins,OBJ_F_ITEM_PARENT)==chest);
    REQUIRE(obj_field_handle_get(dagger,OBJ_F_ITEM_PARENT)==chest);
    REQUIRE(obj_field_int32_get(coins,OBJ_F_GOLD_QUANTITY)==54321);
    REQUIRE(item_wield_get(npc,ITEM_INV_LOC_WEAPON)==wielded);
    REQUIRE(obj_field_handle_get(wielded,OBJ_F_ITEM_PARENT)==npc);
    REQUIRE(sub_464D20(wielded,ITEM_INV_LOC_WEAPON,npc)==ITEM_CANNOT_OK);
    puts("PASS: fresh housing inventory resolves both owners, items, gold quantity and equipped weapon");
    REQUIRE(item_transfer(dagger,pc));
    object_destroy(coins);
    REQUIRE(obj_field_int32_get(chest,OBJ_F_CONTAINER_INVENTORY_NUM)==0);
    for(int pass=0;pass<3;pass++){
        for(int tick=0;tick<3;tick++){gamelib_ping_time+=1000;modern_house_tick();}
        REQUIRE(gamelib_save("Slot0091","Authored inventory regression"));
        REQUIRE(gamelib_load("Slot0091"));pc=player_get_local_pc_obj();
        jump.obj=pc;jump.loc=location_make(61996,65628);REQUIRE(teleport_do(&jump));teleport_ping(0);
        jump.loc=location_make(1346*64+30,1024*64+23);REQUIRE(teleport_do(&jump));teleport_ping(0);
        gamelib_ping_time+=1000;modern_house_tick();
        chest=obj_pool_perm_lookup(inventory_fixture_id(2));dagger=obj_pool_perm_lookup(inventory_fixture_id(4));
        coins=obj_pool_perm_lookup(inventory_fixture_id(3));npc=obj_pool_perm_lookup(inventory_fixture_id(5));wielded=obj_pool_perm_lookup(inventory_fixture_id(6));
        REQUIRE(chest && dagger && npc && wielded);
        REQUIRE(obj_field_int32_get(chest,OBJ_F_CONTAINER_INVENTORY_NUM)==0);
        REQUIRE(!coins || (obj_field_int32_get(coins,OBJ_F_FLAGS)&OF_DESTROYED));
        REQUIRE(obj_field_handle_get(dagger,OBJ_F_ITEM_PARENT)==pc);
        REQUIRE(item_wield_get(npc,ITEM_INV_LOC_WEAPON)==wielded);
        REQUIRE(item_gold_get(pc)==player_gold);
        proxy=obj_pool_perm_lookup(inventory_fixture_id(7));REQUIRE(proxy);authored=obj_field_ptr_get(proxy,OBJ_F_LIGHT_HANDLE);
        REQUIRE(authored && authored->obj==proxy && authored->r==210 && authored->g==90 && authored->b==30 && !(authored->flags&LF_OFF));
        int64_t obj;int iterator;int counts[7]={0};
        if(obj_inst_first(&obj,&iterator))do{for(unsigned i=2;i<=6;i++)if(objid_is_equal(obj_get_id(obj),inventory_fixture_id(i)))counts[i]++;}while(obj_inst_next(&obj,&iterator));
        for(unsigned i=2;i<=6;i++)REQUIRE(counts[i]<=1);
    }
    puts("PASS: consumed gold stays consumed, transferred item stays with player, equipment, attached light color and GUIDs survive repeated ticks and three save/reload/revisits");
    return 0;
}

static ObjectID update_guid(const char* text){ObjectID id={0};unsigned char* raw=(unsigned char*)&id;for(int i=0;i<18;i++){unsigned v=0;sscanf(text+2*i,"%2x",&v);raw[i<2?i:i+6]=(unsigned char)v;}return id;}
static int check_housing_update(const char* mode){
    bool prepare=!strcmp(mode,"--prepare-update-legacy"),legacy=!strcmp(mode,"--check-update-legacy");
    const RwPackage* package=NULL;for(unsigned i=0;i<rw_package_count;i++)if(!strcmp(rw_packages[i].id,"housing.0"))package=&rw_packages[i];
    REQUIRE(prepare?!package:package!=NULL);
    int64_t pc=player_get_local_pc_obj();int world=map_list_info_find("Arcanum1-024-fixed")+1;
    TeleportData jump={.obj=pc,.map=world,.loc=location_make(1346*64+30,1024*64+23)};REQUIRE(teleport_do(&jump));teleport_ping(0);gamelib_ping_time+=1000;modern_house_tick();
    char removed[40]={0};unsigned tile=0,art=0;int64_t victim=0,guard=0;
    if(legacy){FILE* marker=fopen("housing-update.marker","rb");REQUIRE(marker);REQUIRE(fscanf(marker,"%39s %u %u",removed,&tile,&art)==3);fclose(marker);}
    mes_file_handle_t scene;REQUIRE(mes_load("house/scene/scene.mes",&scene));MesFileEntry e;unsigned present=0;
    for(bool more=mes_find_first(scene,&e);more;more=mes_find_next(scene,&e)){char role[20],id[40];if(e.num<100||sscanf(e.str,"%19s %39s",role,id)!=2)continue;int64_t obj=obj_pool_perm_lookup(update_guid(id));
        if(obj&&!(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_DESTROYED))present++;
        if(!strcmp(role,"guard"))guard=obj;
        if(!legacy&&!victim&&obj&&obj_field_int32_get(obj,OBJ_F_TYPE)==OBJ_TYPE_SCENERY){victim=obj;strcpy(removed,id);}
    }mes_unload(scene);REQUIRE(guard);REQUIRE(present>100);
    Sector* sector;int64_t sid=SECTOR_MAKE(1346LL,1024LL);REQUIRE(sector_lock(sid,&sector));
    if(legacy){REQUIRE(sector->tiles.art_ids[tile]==art);victim=obj_pool_perm_lookup(update_guid(removed));REQUIRE(!victim||(obj_field_int32_get(victim,OBJ_F_FLAGS)&OF_DESTROYED));}
    else {mes_file_handle_t terrain;REQUIRE(mes_load("house/scene/terrain.mes",&terrain));REQUIRE(mes_find_first(terrain,&e));tile=(unsigned)e.num;mes_unload(terrain);unsigned original=sector->tiles.art_ids[tile];for(unsigned i=0;i<4096;i++)if(sector->tiles.art_ids[i]!=original){art=sector->tiles.art_ids[i];break;}REQUIRE(art&&art!=original);rw_scene_ground(sector,tile,art);}
    sector_unlock(sid);
    if(!legacy){REQUIRE(victim);object_destroy(victim);FILE* marker=fopen("housing-update.marker","wb");REQUIRE(marker);fprintf(marker,"%s %u %u\n",removed,tile,art);REQUIRE(fclose(marker)==0);}
    if(prepare){REQUIRE(gamelib_save("Slot0090","Legacy house before shared ledger"));puts("PASS: legacy house installed; missing authored GUID and edited floor saved without package ledger");return 0;}
    /* Transfer and consume existing guard inventory; neither may return to its owner. */
    int64_t moved=0,consumed=0;ObjectID moved_id={0},consumed_id={0};int inventory=obj_arrayfield_length_get(guard,OBJ_F_CRITTER_INVENTORY_LIST_IDX);
    for(int i=0;i<inventory;i++){int64_t item=obj_arrayfield_handle_get(guard,OBJ_F_CRITTER_INVENTORY_LIST_IDX,i);if(!item||obj_field_int32_get(item,OBJ_F_TYPE)==OBJ_TYPE_GOLD)continue;if(!moved)moved=item;else if(!consumed){consumed=item;break;}}
    REQUIRE(moved&&consumed);moved_id=obj_get_id(moved);consumed_id=obj_get_id(consumed);REQUIRE(item_transfer(moved,pc));REQUIRE(obj_field_handle_get(moved,OBJ_F_ITEM_PARENT)==pc);object_destroy(consumed);
    for(int pass=0;pass<2;pass++){
        gamelib_ping_time+=1000;modern_house_tick();REQUIRE(gamelib_save("Slot0091","Shared housing migration regression"));REQUIRE(gamelib_load("Slot0091"));pc=player_get_local_pc_obj();
        jump.obj=pc;jump.loc=location_make(61996,65628);REQUIRE(teleport_do(&jump));teleport_ping(0);jump.loc=location_make(1346*64+30,1024*64+23);REQUIRE(teleport_do(&jump));teleport_ping(0);gamelib_ping_time+=1000;modern_house_tick();
        victim=obj_pool_perm_lookup(update_guid(removed));REQUIRE(!victim||(obj_field_int32_get(victim,OBJ_F_FLAGS)&OF_DESTROYED));REQUIRE(sector_lock(sid,&sector));REQUIRE(sector->tiles.art_ids[tile]==art);sector_unlock(sid);
        moved=obj_pool_perm_lookup(moved_id);consumed=obj_pool_perm_lookup(consumed_id);REQUIRE(moved&&obj_field_handle_get(moved,OBJ_F_ITEM_PARENT)==pc);REQUIRE(!consumed||(obj_field_int32_get(consumed,OBJ_F_FLAGS)&OF_DESTROYED));
    }
    printf("PASS: %s housing package preserves missing GUID, edited floor, consumed inventory and player transfer across two save/reload/revisits\n",legacy?"legacy-adopted":"fresh");return 0;
}

int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IONBF,0);
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

    if(argc==4 && (!strcmp(argv[3],"--check-update")||!strcmp(argv[3],"--prepare-update-legacy")||!strcmp(argv[3],"--check-update-legacy"))){REQUIRE(check_housing_update(argv[3])==0);gameuilib_exit();gamelib_exit();tig_exit();return 0;}
    if(argc==4 && !strcmp(argv[3],"--check-inventory")){
        REQUIRE(check_authored_inventory()==0);
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }
    if(argc==4 && !strcmp(argv[3],"--check-overlays")){
        int64_t sid=SECTOR_MAKE(1346LL,1024LL),pc=player_get_local_pc_obj();Sector* sec;Script tile;
        TeleportData jump={.obj=pc,.map=map_current_map(),.loc=location_make(1346*64+30,1024*64+30)};
        REQUIRE(teleport_do(&jump));teleport_ping(0);
        REQUIRE(sector_lock(sid,&sec));
        REQUIRE(sec->blocks.mask[0]&1u);REQUIRE(sec->aptitude_adj==-42);
        REQUIRE(sec->light_scheme==7 && sec->sounds.music_scheme_idx==100 && sec->sounds.ambient_scheme_idx==4100);
        REQUIRE(tile_script_list_get(&sec->tile_scripts,0,&tile));REQUIRE(tile.num==48301 && tile.hdr.counters==0xff030201u);
        REQUIRE(sec->sector_scripts.scr.num==48301);
        tile.hdr.counters=0x01020304u;REQUIRE(tile_script_list_set(&sec->tile_scripts,0,&tile));
        Script saved=sec->sector_scripts.scr;saved.hdr.counters=77;REQUIRE(sector_script_list_set(&sec->sector_scripts,&saved));sector_unlock(sid);
        REQUIRE(gamelib_save("Slot0092","Housing overlays test"));REQUIRE(gamelib_load("Slot0092"));
        REQUIRE(sector_lock(sid,&sec));REQUIRE(sec->blocks.mask[0]&1u);REQUIRE(sec->aptitude_adj==-42);
        REQUIRE(sec->light_scheme==7 && sec->sounds.music_scheme_idx==100 && sec->sounds.ambient_scheme_idx==4100);
        REQUIRE(tile_script_list_get(&sec->tile_scripts,0,&tile));REQUIRE(tile.hdr.counters==0x01020304u);
        REQUIRE(sec->sector_scripts.scr.hdr.counters==77);
        tile.num=0;REQUIRE(tile_script_list_set(&sec->tile_scripts,0,&tile));REQUIRE(sector_script_list_remove(&sec->sector_scripts));sector_unlock(sid);
        REQUIRE(gamelib_save("Slot0092","Housing disabled scripts test"));REQUIRE(gamelib_load("Slot0092"));
        REQUIRE(sector_lock(sid,&sec));REQUIRE(tile_script_list_get(&sec->tile_scripts,0,&tile));REQUIRE(tile.num==0);REQUIRE(sec->sector_scripts.scr.num==0);sector_unlock(sid);
        puts("PASS: housing blockers/environment, script counters and disabled scripts survive two save/reloads");
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }

    if(argc==4 && !strcmp(argv[3],"--check-shadows")){
        location_origin_set(location_make(86175,65557));
        capture("shadows");
        REQUIRE(check_hidden_lights()==0);
        REQUIRE(gamelib_save("Slot0093","Vegetation shadows regression"));
        REQUIRE(gamelib_load("Slot0093"));
        location_origin_set(location_make(86175,65557));
        capture("shadows-reloaded");
        REQUIRE(check_hidden_lights()==0);
        REQUIRE(check_door()==0);
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }

    if(argc==4 && !strcmp(argv[3],"--check-door")){
        REQUIRE(check_door()==0);
        REQUIRE(gamelib_save("Slot0094","Door regression"));
        REQUIRE(gamelib_load("Slot0094"));
        REQUIRE(check_door()==0);
        puts("PASS: door regression after save/reload on isolated copy");
        gameuilib_exit();gamelib_exit();tig_exit();return 0;
    }
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
    REQUIRE(check_hidden_lights()==0);
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
    REQUIRE(check_door()==0);
    gameuilib_exit();gamelib_exit();tig_exit();return 0;
}
