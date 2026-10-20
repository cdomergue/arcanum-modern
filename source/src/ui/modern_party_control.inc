enum {PARTY_DETACHED_LIMIT=128};
static ObjectID party_detached[PARTY_DETACHED_LIMIT];
static int party_detached_count;
static int64_t party_selected;

static bool party_control_member(int64_t obj)
{
    int64_t pc=player_get_local_pc_obj();
    return obj && pc && obj_handle_is_valid(obj)
        && obj_field_int32_get(obj,OBJ_F_TYPE)==OBJ_TYPE_NPC
        && critter_pc_leader_get(obj)==pc;
}

bool modern_party_detached(int64_t obj)
{
    if(!party_control_member(obj))return false;
    ObjectID id=obj_get_id(obj);
    for(int i=0;i<party_detached_count;i++)if(objid_is_equal(id,party_detached[i]))return true;
    return false;
}

bool modern_party_set_grouped(int64_t obj,bool grouped)
{
    if(!party_control_member(obj) || combat_turn_based_is_active()
        || dialog_ui_is_in_dialog(player_get_local_pc_obj()))return false;
    ObjectID id=obj_get_id(obj);
    int index=0;while(index<party_detached_count && !objid_is_equal(id,party_detached[index]))index++;
    if(grouped){
        if(index<party_detached_count)party_detached[index]=party_detached[--party_detached_count];
        if(party_selected==obj)party_selected=OBJ_HANDLE_NULL;
    }else if(index==party_detached_count){
        if(party_detached_count==PARTY_DETACHED_LIMIT || !sub_424070(obj,3,0,0))return false;
        party_detached[party_detached_count++]=id;
    }
    return true;
}

int64_t modern_party_selected(void)
{
    if(combat_turn_based_is_active() || !modern_party_detached(party_selected)
        || !critter_is_active(party_selected)
        || (obj_field_int32_get(party_selected,OBJ_F_CRITTER_FLAGS)&(OCF_FLEEING|OCF_SPELL_FLEE|OCF_SURRENDERED))
        || (obj_field_int32_get(party_selected,OBJ_F_SPELL_FLAGS)&OSF_MIND_CONTROLLED))party_selected=OBJ_HANDLE_NULL;
    return party_selected;
}

bool modern_party_select(int64_t obj)
{
    if(dialog_ui_is_in_dialog(player_get_local_pc_obj()))return false;
    if(obj!=party_selected && (intgame_mode_get()==INTGAME_MODE_SKILL || intgame_mode_get()==INTGAME_MODE_SPELL || intgame_mode_get()==INTGAME_MODE_ITEM))intgame_mode_set(INTGAME_MODE_MAIN);
    if(obj!=party_selected)dialog_speaker_set(OBJ_HANDLE_NULL);
    if(obj==OBJ_HANDLE_NULL){party_selected=OBJ_HANDLE_NULL;return true;}
    if(!party_control_member(obj) || !critter_is_active(obj)
        || (obj_field_int32_get(obj,OBJ_F_SPELL_FLAGS)&OSF_MIND_CONTROLLED)
        || !modern_party_set_grouped(obj,false))return false;
    party_selected=obj;
    return modern_party_selected()==obj;
}

static bool party_control_save(void)
{
    TigFile* file=tig_file_fopen("Save\\Current\\modern-party.dat","wb");
    if(!file)return false;
    int version=1;
    bool ok=tig_file_fwrite(&version,sizeof(version),1,file)==1
        && tig_file_fwrite(&party_detached_count,sizeof(party_detached_count),1,file)==1;
    for(int i=0;ok && i<party_detached_count;i++)ok=tig_file_fwrite(&party_detached[i],sizeof(ObjectID),1,file)==1;
    if(tig_file_fclose(file))ok=false;
    return ok;
}

static bool party_control_load(void)
{
    party_detached_count=0;party_selected=OBJ_HANDLE_NULL;
    TigFile* file=tig_file_fopen("Save\\Current\\modern-party.dat","rb");
    if(!file)return true;
    int version,count;
    bool ok=tig_file_fread(&version,sizeof(version),1,file)==1 && version==1
        && tig_file_fread(&count,sizeof(count),1,file)==1 && count>=0 && count<=PARTY_DETACHED_LIMIT;
    for(int i=0;ok && i<count;i++){
        ok=tig_file_fread(&party_detached[i],sizeof(ObjectID),1,file)==1;
        for(int j=0;ok && j<i;j++)if(objid_is_equal(party_detached[i],party_detached[j]))ok=false;
    }
    tig_file_fclose(file);
    if(ok)party_detached_count=count;
    return ok;
}
