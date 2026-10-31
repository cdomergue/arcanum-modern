/* Native regression. Only marked disposable installations may be used. */
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
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/obj_pool.h"
#include "game/proto.h"
#include "game/item.h"
#include "game/location.h"
#include "game/sector.h"
#include "game/map.h"
#include "game/teleport.h"
#include "game/terrain.h"
#include "game/quest.h"
#include "game/stat.h"
#include "game/script.h"
#include "game/critter.h"
#include "game/modern_world_updates_internal.h"
#include "ui/iso.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"Failed: %s (line %d); update error: %s\n",#x,__LINE__,modern_world_updates_error());return 1;}}while(0)
static ObjectID fixture_id(unsigned n)
{
    ObjectID id={0};id.type=OID_TYPE_GUID;for(unsigned i=0;i<16;i++)id.d.g.data[i]=(unsigned char)(0x80+n*16+i);return id;
}
static int64_t lookup(unsigned n){return obj_pool_perm_lookup(fixture_id(n));}
static int64_t origin(void){return location_make(1345*64+30,1024*64+30);}
static int64_t destination(void){return location_make(1345*64+32,1024*64+30);}
static int validate_operations(void)
{
    char error[256];RwOperation op={0};op.kind=RW_MOVE;op.selector=RW_GUID;op.type=2;op.prototype=3044;op.sector=sector_id_from_loc(origin());
    strcpy(op.guid,"0200808182838485868788898a8b8c8d8e8f");op.before[0]=origin();op.after[0]=destination();REQUIRE(rw_operation_validate(&op,error,sizeof(error)));
    op.selector=RW_STATIC;REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));op.selector=RW_GUID;
    op.after[0]=location_make(61996,65628);REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));op.after[0]=destination();
    op.kind=RW_FIELD;op.before[0]=0;op.after[0]=1;strcpy(op.field,"CONTAINER_KEY_ID");REQUIRE(rw_operation_validate(&op,error,sizeof(error)));
    strcpy(op.field,"ITEM_PARENT");REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));
    op.kind=RW_FLAGS;strcpy(op.field,"FLAGS");op.mask=OF_DYNAMIC;REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));
    op.mask=OF_OFF;op.after[0]=OF_OFF;REQUIRE(rw_operation_validate(&op,error,sizeof(error)));
    op.kind=RW_BLOCK;op.selector=RW_NONE;op.index=4096;REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));op.index=0;op.after[0]=1;REQUIRE(!rw_operation_validate(&op,error,sizeof(error)));
    strcpy(op.base_path,"maps/test/base.sec");memset(op.base_hash,1,32);REQUIRE(rw_operation_validate(&op,error,sizeof(error)));
    puts("PASS: native validators reject static/cross-sector moves, identity fields, dynamic flags, invalid cells and absent base provenance");return 0;
}
static int quiet_quests(void)
{
    int64_t pc=player_get_local_pc_obj();int ids[3],count=0;char error[256];
    for(int num=1000;num<2000&&count<3;num++)if(quest_global_state_get(num)==QUEST_STATE_ACCEPTED){
        int state=quest_state_get(pc,num);
        if(quest_state_migrate(pc,num,state,state,false,error,sizeof(error)))ids[count++]=num;
    }
    REQUIRE(count==3);int xp=stat_base_get(pc,STAT_EXPERIENCE_POINTS),alignment=stat_base_get(pc,STAT_ALIGNMENT);
    PcQuestState state={0},result;memset(&state.datetime,0x12,sizeof(state.datetime));state.padding_C=0x12345678;
    for(int i=0;i<3;i++){state.state=QUEST_STATE_UNKNOWN;obj_arrayfield_pc_quest_set(pc,OBJ_F_PC_QUEST_IDX,ids[i],&state);}
    REQUIRE(!quest_state_migrate(pc,ids[0],QUEST_STATE_ACCEPTED,QUEST_STATE_ACHIEVED,false,error,sizeof(error)));
    REQUIRE(quest_state_get(pc,ids[0])==QUEST_STATE_UNKNOWN);
    REQUIRE(quest_state_migrate(pc,ids[0],QUEST_STATE_UNKNOWN,QUEST_STATE_ACCEPTED,false,error,sizeof(error)));
    REQUIRE(quest_state_migrate(pc,ids[0],QUEST_STATE_ACCEPTED,QUEST_STATE_ACHIEVED,false,error,sizeof(error)));
    obj_arrayfield_pc_quest_get(pc,OBJ_F_PC_QUEST_IDX,ids[0],&result);REQUIRE(!memcmp(&state.datetime,&result.datetime,sizeof(state.datetime))&&result.padding_C==state.padding_C);
    REQUIRE(!quest_state_migrate(pc,ids[0],QUEST_STATE_ACHIEVED,QUEST_STATE_COMPLETED,true,error,sizeof(error)));
    REQUIRE(quest_state_migrate(pc,ids[1],QUEST_STATE_UNKNOWN,QUEST_STATE_COMPLETED,false,error,sizeof(error)));
    REQUIRE(!quest_state_migrate(pc,ids[1],QUEST_STATE_COMPLETED,QUEST_STATE_ACCEPTED,false,error,sizeof(error)));
    REQUIRE(quest_state_migrate(pc,ids[2],QUEST_STATE_UNKNOWN,QUEST_STATE_BOTCHED,false,error,sizeof(error)));
    REQUIRE(!quest_state_migrate(pc,ids[2],QUEST_STATE_BOTCHED,QUEST_STATE_UNKNOWN,false,error,sizeof(error)));
    REQUIRE(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==xp&&stat_base_get(pc,STAT_ALIGNMENT)==alignment);
    REQUIRE(gamelib_save("Slot0094","Quiet quest conversion regression"));REQUIRE(gamelib_load("Slot0094"));pc=player_get_local_pc_obj();
    REQUIRE(quest_state_get(pc,ids[0])==QUEST_STATE_ACHIEVED&&quest_state_get(pc,ids[1])==QUEST_STATE_COMPLETED&&quest_state_get(pc,ids[2])==QUEST_STATE_BOTCHED);
    REQUIRE(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==xp&&stat_base_get(pc,STAT_ALIGNMENT)==alignment);
    obj_arrayfield_pc_quest_get(pc,OBJ_F_PC_QUEST_IDX,ids[0],&result);REQUIRE(!memcmp(&state.datetime,&result.datetime,sizeof(state.datetime))&&result.padding_C==state.padding_C);
    puts("PASS: quiet quest migration preserves dates/padding/XP/alignment; wrong guards, authoritative writes and terminal resets refused; states survive save/reload");return 0;
}
static int prepare_globals(void)
{
    int64_t pc=player_get_local_pc_obj();int ids[3],count=0;char error[256];
    for(int num=1000;num<2000&&count<3;num++)if(quest_global_state_get(num)==QUEST_STATE_ACCEPTED){
        int current=quest_state_get(pc,num);if(quest_state_migrate(pc,num,current,current,false,error,sizeof(error)))ids[count++]=num;
    }
    REQUIRE(count==3);PcQuestState state={0};
    for(int i=0;i<3;i++){state.state=i==0?QUEST_STATE_UNKNOWN:i==1?QUEST_STATE_COMPLETED:0x100;obj_arrayfield_pc_quest_set(pc,OBJ_F_PC_QUEST_IDX,ids[i],&state);}
    script_global_var_set(1900,321);script_global_flag_set(3100,0);
    FILE* file=fopen("world-update-globals.json","w");REQUIRE(file);fprintf(file,"{\"quests\":[%d,%d,%d]}\n",ids[0],ids[1],ids[2]);REQUIRE(fclose(file)==0);
    REQUIRE(gamelib_save("Slot0092","Cumulative update globals baseline"));puts("PASS: guarded globals and mixed quest states prepared before skipped revision update");return 0;
}
static int verify_globals(void)
{
    int ids[3];FILE* file=fopen("world-update-globals.json","r");REQUIRE(file);REQUIRE(fscanf(file,"{\"quests\":[%d,%d,%d]}",&ids[0],&ids[1],&ids[2])==3);fclose(file);
    int64_t pc=player_get_local_pc_obj();REQUIRE(script_global_var_get(1900)==654&&script_global_flag_get(3100)==1);
    REQUIRE(quest_state_get(pc,ids[0])==QUEST_STATE_ACCEPTED&&quest_state_get(pc,ids[1])==QUEST_STATE_COMPLETED&&quest_state_get(pc,ids[2])==QUEST_STATE_BOTCHED);
    unsigned applied=0,conflicts=0;for(unsigned i=0;i<rw_package_count;i++)if(!strcmp(rw_packages[i].id,"native-regression")){
        REQUIRE(rw_packages[i].revision==3);
        for(unsigned j=0;j<rw_packages[i].operation_count;j++)if(rw_packages[i].operations[j].kind==RW_GLOBAL){int state=rw_state_outcome(&rw_packages[i],&rw_packages[i].operations[j]);if(state==RW_APPLIED)applied++;if(state==RW_CONFLICT)conflicts++;}
    }
    REQUIRE(applied==3&&conflicts==2);
    script_global_var_set(1900,999);script_global_flag_set(3100,0);char error[256];REQUIRE(quest_state_migrate(pc,ids[0],QUEST_STATE_ACCEPTED,QUEST_STATE_ACHIEVED,false,error,sizeof(error)));
    REQUIRE(gamelib_save("Slot0093","Cumulative update globals replay"));REQUIRE(gamelib_load("Slot0093"));pc=player_get_local_pc_obj();
    REQUIRE(script_global_var_get(1900)==999&&script_global_flag_get(3100)==0&&quest_state_get(pc,ids[0])==QUEST_STATE_ACHIEVED);
    REQUIRE(quest_state_get(pc,ids[1])==QUEST_STATE_COMPLETED&&quest_state_get(pc,ids[2])==QUEST_STATE_BOTCHED);
    puts("PASS: cumulative revision 1 to 3 applies guarded globals/quest; terminal quests conflict; player changes survive replay without revision 2");return 0;
}
static int prepare_actors(void)
{
    for(unsigned i=3;i<6;i++){
        REQUIRE(!lookup(i));int64_t npc;obj_create_inst_with_oid(sub_4685A0(17305),origin(),fixture_id(i),&npc);REQUIRE(npc);
        obj_field_int32_set(npc,OBJ_F_NPC_FLAGS,ONF_KOS_OVERRIDE);REQUIRE(object_install_loaded(npc));
        char path[80];snprintf(path,sizeof(path),"world-update-actor-%u.mob",i);TigFile* stream=tig_file_fopen(path,"wb");REQUIRE(stream);
        obj_save_preprocess(npc);REQUIRE(obj_write(stream,npc));REQUIRE(tig_file_fclose(stream)==0);obj_load_postprocess(npc);
        if(i==3)obj_field_int32_set(npc,OBJ_F_HP_DAMAGE,32000);
        if(i==4)REQUIRE(critter_follow(npc,player_get_local_pc_obj(),true));
        if(i==5)sub_43E770(npc,location_make(1344*64+30,1024*64+30),0,0);
    }
    REQUIRE(gamelib_save("Slot0095","Existing actors update baseline"));puts("PASS: dead, recruited and moved existing GUID actors prepared before additive package");return 0;
}
static int verify_actors(void)
{
    for(int pass=0;pass<2;pass++){
        int64_t dead=lookup(3),follower=lookup(4),moved=lookup(5);REQUIRE(dead&&follower&&moved);
        REQUIRE(critter_is_dead(dead)&&obj_field_int32_get(dead,OBJ_F_HP_DAMAGE)==32000);
        REQUIRE(critter_pc_leader_get(follower)==player_get_local_pc_obj());
        REQUIRE(obj_field_int64_get(moved,OBJ_F_LOCATION)==location_make(1344*64+30,1024*64+30));
        int64_t obj;int iterator;unsigned counts[3]={0};
        if(obj_inst_first(&obj,&iterator))do{ObjectID id=obj_get_id(obj);for(unsigned i=3;i<6;i++)if(objid_is_equal(id,fixture_id(i)))counts[i-3]++;}while(obj_inst_next(&obj,&iterator));
        for(unsigned i=0;i<3;i++)REQUIRE(counts[i]==1);
        REQUIRE(gamelib_save("Slot0096","Existing actors replay"));REQUIRE(gamelib_load("Slot0096"));
    }
    puts("PASS: existing GUID adoption preserves death, recruitment and moved NPC location without duplicate actors across save/reload");return 0;
}
static int prepare(void)
{
    int64_t chest,item;REQUIRE(!lookup(0));
    obj_create_inst_with_oid(sub_4685A0(3044),origin(),fixture_id(0),&chest);REQUIRE(chest);REQUIRE(object_install_loaded(chest));
    obj_field_int32_set(chest,OBJ_F_CONTAINER_KEY_ID,61039);obj_field_int32_set(chest,OBJ_F_CONTAINER_LOCK_DIFFICULTY,77);
    obj_create_inst_with_oid(sub_4685A0(6049),origin(),fixture_id(1),&item);REQUIRE(item);REQUIRE(object_install_loaded(item));REQUIRE(item_transfer(item,chest));
    int64_t sid=sector_id_from_loc(origin());Sector* sector;REQUIRE(sector_lock(sid,&sector));
    int64_t source=0;for(int i=0;i<4096&&!source;i++)for(ObjectNode* n=sector->objects.heads[i];n;n=n->next)if(obj_field_int32_get(n->obj,OBJ_F_TYPE)==OBJ_TYPE_SCENERY){source=n->obj;break;}
    REQUIRE(source);int64_t proto=obj_field_handle_get(source,OBJ_F_PROTOTYPE_HANDLE);unsigned prototype=(unsigned)obj_get_id(proto).d.a;
    unsigned block=sector->blocks.mask[0]&1;sector_unlock(sid);
    int64_t decor;obj_create_inst_with_oid(proto,location_make(1345*64+35,1024*64+30),fixture_id(2),&decor);REQUIRE(decor);
    obj_field_int32_set(decor,OBJ_F_FLAGS,obj_field_int32_get(decor,OBJ_F_FLAGS)|OF_DYNAMIC);
    TigFile* stream=tig_file_fopen("world-update-decor.mob","wb");REQUIRE(stream);obj_save_preprocess(decor);REQUIRE(obj_write(stream,decor));REQUIRE(tig_file_fclose(stream)==0);obj_deallocate(decor);
    FILE* file=fopen("world-update-fixture.json","w");REQUIRE(file);char* map;REQUIRE(map_get_name(map_current_map(),&map));
    char base[260],hex[65];uint8_t hash[32];snprintf(base,sizeof(base),"maps/%s/%llu.sec",map,(unsigned long long)sid);
    if(!tig_file_exists(base,NULL))terrain_sector_path(sid,base);
    REQUIRE(rw_hash_resource(base,hash));for(int i=0;i<32;i++)sprintf(hex+i*2,"%02x",hash[i]);
    for(char* c=base;*c;c++){if(*c=='\\')*c='/';else if(*c>='A'&&*c<='Z')*c+='a'-'A';}
    fprintf(file,"{\"map\":\"%s\",\"sector\":%llu,\"before_location\":%llu,\"after_location\":%llu,\"block_before\":%u,\"decor_prototype\":%u,\"base_path\":\"%s\",\"base_sha256\":\"%s\"}\n",map,(unsigned long long)sid,(unsigned long long)origin(),(unsigned long long)destination(),block,prototype,base,hex);REQUIRE(fclose(file)==0);
    REQUIRE(gamelib_save("Slot0089","World update baseline"));puts("PASS: baseline chest, lock and inventory saved before package installation");return 0;
}
static int verify(void)
{
    int64_t chest=lookup(0),item=lookup(1),decor=lookup(2),pc=player_get_local_pc_obj();REQUIRE(chest&&item&&decor);
    REQUIRE(obj_field_int64_get(chest,OBJ_F_LOCATION)==destination());
    REQUIRE(obj_field_int32_get(chest,OBJ_F_CONTAINER_KEY_ID)==61039&&obj_field_int32_get(chest,OBJ_F_CONTAINER_LOCK_DIFFICULTY)==77);
    REQUIRE(obj_field_handle_get(item,OBJ_F_ITEM_PARENT)==chest);
    const RwOperation* block=NULL;
    for(unsigned i=0;i<rw_package_count;i++)for(unsigned j=0;j<rw_packages[i].operation_count;j++)if(rw_packages[i].operations[j].kind==RW_BLOCK)block=&rw_packages[i].operations[j];
    REQUIRE(block);unsigned expected_block=(unsigned)block->after[0];Sector* sector;REQUIRE(sector_lock(sector_id_from_loc(origin()),&sector));REQUIRE((sector->blocks.mask[0]&1)==expected_block);sector_unlock(sector->id);
    REQUIRE(item_transfer(item,pc));object_destroy(decor);
    /* A later player movement must survive the already-applied operation. */
    int64_t player_location=location_make(1345*64+33,1024*64+31);sub_43E770(chest,player_location,3,-2);
    for(int pass=0;pass<3;pass++){
        REQUIRE(gamelib_save("Slot0090","World update replay test"));REQUIRE(gamelib_load("Slot0090"));
        pc=player_get_local_pc_obj();TeleportData jump={.obj=pc,.map=map_current_map(),.loc=location_make(61996,65628)};REQUIRE(teleport_do(&jump));teleport_ping(0);jump.loc=origin();REQUIRE(teleport_do(&jump));teleport_ping(0);
        chest=lookup(0);item=lookup(1);decor=lookup(2);REQUIRE(chest&&item);REQUIRE(!decor||(obj_field_int32_get(decor,OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT)));
        REQUIRE(obj_field_int64_get(chest,OBJ_F_LOCATION)==player_location&&obj_field_int32_get(chest,OBJ_F_OFFSET_X)==3&&obj_field_int32_get(chest,OBJ_F_OFFSET_Y)==-2);
        REQUIRE(obj_field_handle_get(item,OBJ_F_ITEM_PARENT)==pc);REQUIRE(obj_field_int32_get(chest,OBJ_F_CONTAINER_KEY_ID)==61039&&obj_field_int32_get(chest,OBJ_F_CONTAINER_LOCK_DIFFICULTY)==77);
        REQUIRE(sector_lock(sector_id_from_loc(origin()),&sector));REQUIRE((sector->blocks.mask[0]&1)==expected_block);sector_unlock(sector->id);
    }
    puts("PASS: move preserves inventory/locks; block persists; consumed addition never respawns; player changes survive three save/reload/revisit cycles");return 0;
}
int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IONBF,0);if(argc==2&&!strcmp(argv[1],"--validate"))return validate_operations();if(argc!=4||chdir(argv[1])||access(".renaissance-house-test",R_OK))return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    highres_config_load();TigInitInfo init={0};init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;REQUIRE(tig_init(&init)==TIG_OK);
    intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;
    REQUIRE(intgame_create_iso_window(&game.iso_window_handle));REQUIRE(gamelib_init(&game));REQUIRE(gameuilib_init(&game));REQUIRE(gamelib_mod_load("Arcanum"));REQUIRE(gameuilib_mod_load());gamelib_splash_clear();REQUIRE(modern_ui_init());
    REQUIRE(gamelib_load(argv[2]));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    TeleportData jump={.obj=player_get_local_pc_obj(),.map=map_list_info_find("Arcanum1-024-fixed")+1,.loc=origin()};REQUIRE(teleport_do(&jump));teleport_ping(0);
    int result=!strcmp(argv[3],"--prepare")?prepare():!strcmp(argv[3],"--verify")?verify():!strcmp(argv[3],"--quests")?quiet_quests():!strcmp(argv[3],"--prepare-globals")?prepare_globals():!strcmp(argv[3],"--verify-globals")?verify_globals():!strcmp(argv[3],"--prepare-actors")?prepare_actors():!strcmp(argv[3],"--verify-actors")?verify_actors():2;
    gameuilib_exit();gamelib_exit();tig_exit();return result;
}
