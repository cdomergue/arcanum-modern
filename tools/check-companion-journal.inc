#include "game/ai.h"
static const CompanionEntry* check_companion(ObjectID id)
{
    for(int i=0;i<modern_companions_count();i++) {
        const CompanionEntry* e=modern_companions_get(i);
        if(objid_is_equal(id,e->id))return e;
    }
    return NULL;
}
static int check_object_count(void)
{
    int n=0,iter;int64_t obj;
    if(obj_inst_first(&obj,&iter))do{n++;}while(obj_inst_next(&obj,&iter));
    return n;
}
static void check_companion_journal(void)
{
    int64_t pc=player_get_local_pc_obj(),followers[128];
    int n=modern_followers_get(followers,128);assert(n>0);
    int64_t npc=followers[0];ObjectID id=obj_get_id(npc);
    int before=check_object_count();modern_companions_refresh();
    assert(check_object_count()==before);assert(check_companion(id));
    printf("Companion recovery: %d entries, no new objects\n",modern_companions_count());
    assert(gamelib_save("Slot9901","Companion journal baseline"));
    ai_npc_wait(npc);assert(check_companion(id)->state==COMPANION_WAITING);
    assert(gamelib_save("Slot9902","Companion journal waiting"));
    assert(gamelib_load("Slot9901"));modern_companions_refresh();
    assert(check_companion(id)->state==COMPANION_PRESENT);
    assert(gamelib_load("Slot9902"));modern_companions_refresh();
    assert(check_companion(id)->state==COMPANION_WAITING);
    npc=obj_pool_perm_lookup(id);pc=player_get_local_pc_obj();assert(npc && pc);
    TimeEvent event={0};event.params[0].object_value=npc;
    assert(ai_npc_wait_here_timeevent_process(&event));
    assert(check_companion(id)->state==COMPANION_WAIT_EXPIRED);
    modern_companions_refresh();assert(check_companion(id)->state==COMPANION_WAIT_EXPIRED);
    assert(critter_follow(npc,pc,true));assert(check_companion(id)->state==COMPANION_PRESENT);
    ai_npc_wait(npc);ai_npc_unwait(npc,true);assert(check_companion(id)->state==COMPANION_PRESENT);
    assert(critter_disband(npc,true));assert(check_companion(id)->state==COMPANION_LEFT);
    assert(critter_follow(npc,pc,true));

    /* Parse the native writer's full and differential formats, including null
     * leader overrides, without registering another object. */
    const char* scratch="Save\\Current\\companion-reader-test.bin";
    TigFile* f=tig_file_fopen(scratch,"wb");assert(f);
    obj_save_preprocess(npc);assert(obj_write(f,npc));obj_load_postprocess(npc);assert(tig_file_fclose(f)==0);
    CompanionSnapshot snapshot;
    f=tig_file_fopen(scratch,"rb");assert(f && obj_companion_snapshot(f,&snapshot));
    long full_size=tig_file_ftell(f);assert(full_size==tig_file_filelength(f));tig_file_fclose(f);
    assert(objid_is_equal(snapshot.id,id) && objid_is_equal(snapshot.leader,obj_get_id(pc)));
    assert(snapshot.type==OBJ_TYPE_NPC && snapshot.description==obj_field_int32_get(npc,OBJ_F_DESCRIPTION));
    f=tig_file_fopen(scratch,"rb");assert(f);
    unsigned char* encoded=malloc((size_t)full_size);assert(encoded);
    assert(tig_file_fread(encoded,1,(size_t)full_size,f)==(size_t)full_size);tig_file_fclose(f);
    const long cuts[]={0,3,27,51,57,58,64,full_size-1};
    for(unsigned i=0;i<sizeof(cuts)/sizeof(*cuts);i++) {
        assert(cuts[i]<full_size);f=tig_file_fopen(scratch,"wb");assert(f);
        if(cuts[i])assert(tig_file_fwrite(encoded,1,cuts[i],f)==(size_t)cuts[i]);tig_file_fclose(f);
        CompanionSnapshot truncated;
        f=tig_file_fopen(scratch,"rb");assert(f && !obj_companion_snapshot(f,&truncated));tig_file_fclose(f);
    }
    free(encoded);
    assert(critter_disband(npc,true));f=tig_file_fopen(scratch,"wb");assert(f);
    obj_save_preprocess(npc);assert(obj_dif_write(f,npc));obj_load_postprocess(npc);assert(tig_file_fclose(f)==0);
    f=tig_file_fopen(scratch,"rb");assert(f && obj_companion_diff(f,&snapshot));tig_file_fclose(f);
    assert(snapshot.leader.type==OID_TYPE_NULL);assert(critter_follow(npc,pc,true));
    f=tig_file_fopen(scratch,"wb");assert(f);int bad_version=118;
    assert(tig_file_fwrite(&bad_version,4,1,f)==1);tig_file_fclose(f);
    f=tig_file_fopen(scratch,"rb");assert(f && !obj_companion_snapshot(f,&snapshot));tig_file_fclose(f);
    tig_file_remove(scratch);

    /* Three off-map NPCs with unrelated generated IDs: a waiting companion,
     * an unrelated jilted NPC, and a companion belonging to another player. */
    int other_map=0;char folder[TIG_MAX_PATH],path[TIG_MAX_PATH];char* map_name;
    for(int i=1;map_get_name(i,&map_name);i++)if(i!=map_current_map()) {
        snprintf(folder,sizeof(folder),"Save\\Current\\maps\\%s",map_name);
        if(!tig_file_is_directory(folder)){other_map=i;break;}
    }
    assert(other_map);tig_file_mkdir(folder);
    snprintf(path,sizeof(path),"%s\\mobile.mdy",folder);f=tig_file_fopen(path,"wb");assert(f);
    ObjectID fixture[3];int64_t prototype=obj_field_handle_get(npc,OBJ_F_PROTOTYPE_HANDLE);
    for(int i=0;i<3;i++) {
        int64_t obj;obj_create_inst(prototype,obj_field_int64_get(pc,OBJ_F_LOCATION),&obj);
        fixture[i]=obj_get_id(obj);
        obj_field_int32_set(obj,OBJ_F_NPC_FLAGS,i==1?ONF_JILTED:ONF_AI_WAIT_HERE);
        if(i!=1)critter_leader_set(obj,i==2?npc:pc);
        obj_save_preprocess(obj);assert(obj_write(f,obj));obj_load_postprocess(obj);obj_deallocate(obj);
    }
    assert(tig_file_fclose(f)==0);
    /* Remove only the scratch registry in this disposable test copy to model
     * an older save. The original slot remains unchanged. */
    assert(tig_file_remove("Save\\Current\\renaissance-companions.dat")==0);
    modern_companions_reset();before=check_object_count();modern_companions_refresh();
    assert(check_object_count()==before);
    assert(check_companion(fixture[0]) && check_companion(fixture[0])->state==COMPANION_WAITING);
    assert(check_companion(fixture[0])->map==other_map && check_companion(fixture[0])->recovered);
    assert(!check_companion(fixture[1]) && !check_companion(fixture[2]));
    assert(modern_companions_save());modern_companions_reset();modern_companions_refresh();
    assert(check_companion(fixture[0]));

    int64_t departed;obj_create_inst(prototype,obj_field_int64_get(pc,OBJ_F_LOCATION),&departed);
    ObjectID departed_id=obj_get_id(departed);
    modern_companions_event(departed,pc,COMPANION_LEFT);obj_deallocate(departed);
    assert(modern_companions_save());
    f=tig_file_fopen("Save\\Current\\renaissance-companions.dat","r+b");assert(f);
    assert(tig_file_fseek(f,12,SEEK_SET)==0 && tig_file_fwrite(&fixture[2],24,1,f)==1);tig_file_fclose(f);
    modern_companions_reset();modern_companions_refresh();assert(!check_companion(departed_id));

    logbook_ui_open(pc);assert(journal);click_action(107);
    const int scales[]={100,150};
    for(int i=0;i<2;i++) {
        text_scale=scales[i];dirty=true;spread=0;char name[80];
        snprintf(name,sizeof(name),"companions-%d-first",scales[i]);capture_frame(name);
        assert(matched_count==modern_companions_count());
        spread=spread_count-1;snprintf(name,sizeof(name),"companions-%d-last",scales[i]);capture_frame(name);
    }
    SDL_strlcpy(search,"zzzz-no-companion",sizeof(search));dirty=true;capture_frame("companions-search-empty");
    assert(matched_count==0);search[0]=0;dirty=true;
    /* Exercise the actual English translation branch without changing any
     * installed language resources. Only the in-memory language sentinel changes. */
    char* language=(char*)modern_intgame_text(0);char original_language[64];
    assert(strlen(language)>=5 && strlen(language)<sizeof(original_language));
    strcpy(original_language,language);strcpy(language,"Level");
    text_scale=150;spread=0;dirty=true;capture_frame("companions-en-150");
    strcpy(language,original_language);dirty=true;
    SDL_SetWindowSize(window,800,600);text_scale=150;spread=0;capture_frame("companions-800x600-150");
    logbook_ui_close();
    /* Returning to the old branch removes the off-map imported fixture. */
    assert(gamelib_load("Slot9901"));modern_companions_refresh();assert(!check_companion(fixture[0]));
    puts("PASS: companion journal native events, save branches, full/diff metadata, old-save off-map recovery, owner isolation and rendering");
}
