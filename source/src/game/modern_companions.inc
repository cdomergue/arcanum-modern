/* Save-scoped last-known companion state. Never writes to a player's save slot:
 * gamelib archives this optional sidecar with the normal Save/Current files. */
#define COMPANION_LIMIT 128
#define COMPANION_FILE "Save\\Current\\renaissance-companions.dat"
static CompanionEntry companion_entries[COMPANION_LIMIT];
static int companion_count;
static ObjectID companion_owner;
static bool companion_loaded, companion_scanned;

void modern_companions_reset(void)
{
    companion_count=0;companion_loaded=false;companion_scanned=false;
    memset(&companion_owner,0,sizeof(companion_owner));
}
static bool companion_id_valid(ObjectID id)
{
    return id.type==OID_TYPE_GUID || id.type==OID_TYPE_P;
}
static bool companion_begin(void)
{
    int64_t pc=player_get_local_pc_obj();
    if(gamelib_in_load() || gamelib_in_reset() || pc==OBJ_HANDLE_NULL)return false;
    ObjectID owner=obj_get_id(pc);
    if(!companion_id_valid(owner))return false;
    if(companion_loaded && objid_is_equal(owner,companion_owner))return true;
    modern_companions_reset();companion_loaded=true;companion_owner=owner;
    TigFile* f=tig_file_fopen(COMPANION_FILE,"rb");
    if(!f)return true;
    uint32_t header[3];ObjectID saved_owner;
    bool ok=tig_file_fread(header,4,3,f)==3 && header[0]==0x434A5241 && header[1]==1
        && header[2]<=COMPANION_LIMIT && tig_file_fread(&saved_owner,24,1,f)==1
        && objid_is_equal(owner,saved_owner);
    for(unsigned i=0;ok && i<header[2];i++) {
        CompanionEntry* e=&companion_entries[i];int32_t values[4];
        ok=tig_file_fread(&e->id,24,1,f)==1 && tig_file_fread(values,4,4,f)==4
            && tig_file_fread(&e->location,8,1,f)==1;
        if(!ok)break;
        e->description=values[0];e->map=values[1];e->state=values[2];e->recovered=values[3];
        char* name=NULL;
        ok=companion_id_valid(e->id) && e->description>=0 && map_get_name(e->map,&name)
            && e->state>=COMPANION_PRESENT && e->state<=COMPANION_UNAVAILABLE
            && (e->recovered==0 || e->recovered==1);
        for(unsigned j=0;ok && j<i;j++)if(objid_is_equal(e->id,companion_entries[j].id))ok=false;
    }
    if(ok && tig_file_ftell(f)!=tig_file_filelength(f))ok=false;
    if(ok)companion_count=(int)header[2];
    tig_file_fclose(f);
    return true;
}
static int companion_index(ObjectID id)
{
    for(int i=0;i<companion_count;i++)if(objid_is_equal(id,companion_entries[i].id))return i;
    return -1;
}
static CompanionEntry* companion_put(ObjectID id)
{
    int i=companion_index(id);
    if(i>=0)return &companion_entries[i];
    if(!companion_id_valid(id) || companion_count==COMPANION_LIMIT)return NULL;
    CompanionEntry* e=&companion_entries[companion_count++];memset(e,0,sizeof(*e));e->id=id;
    return e;
}
void modern_companions_event(int64_t npc,int64_t leader,int state)
{
    if(!companion_begin() || leader==OBJ_HANDLE_NULL || npc==OBJ_HANDLE_NULL
        || !objid_is_equal(obj_get_id(leader),companion_owner)
        || obj_field_int32_get(npc,OBJ_F_TYPE)!=OBJ_TYPE_NPC || map_current_map()<=0)return;
    CompanionEntry* e=companion_put(obj_get_id(npc));if(!e)return;
    e->description=obj_field_int32_get(npc,OBJ_F_DESCRIPTION);
    e->map=map_current_map();e->location=obj_field_int64_get(npc,OBJ_F_LOCATION);
    e->state=critter_is_dead(npc)?COMPANION_DEAD:state;e->recovered=0;
}
void modern_companions_import(const CompanionSnapshot* s,int map)
{
    /* JILTED alone is not proof that this NPC ever followed this player. */
    if(s->type!=OBJ_TYPE_NPC || !(s->flags&ONF_AI_WAIT_HERE)
        || !objid_is_equal(s->leader,companion_owner)
        || (s->object_flags&(OF_DESTROYED|OF_EXTINCT|OF_OFF)) || companion_index(s->id)>=0)return;
    CompanionEntry* e=companion_put(s->id);if(!e)return;
    e->description=s->description;e->map=map;e->location=s->location;
    e->state=COMPANION_WAITING;e->recovered=1;
}
void modern_companions_refresh(void)
{
    if(!companion_begin())return;
    if(!companion_scanned){companion_scanned=true;modern_companions_scan();}
    int iter;int64_t npc;
    if(obj_inst_first(&npc,&iter))do {
        if(obj_field_int32_get(npc,OBJ_F_TYPE)!=OBJ_TYPE_NPC)continue;
        int64_t leader=critter_leader_get(npc);ObjectID id=obj_get_id(npc);
        int index=companion_index(id);
        bool ours=leader!=OBJ_HANDLE_NULL && objid_is_equal(obj_get_id(leader),companion_owner);
        if(!ours && index<0)continue;
        CompanionEntry* e=companion_put(id);if(!e)continue;
        e->description=obj_field_int32_get(npc,OBJ_F_DESCRIPTION);
        e->map=map_current_map();e->location=obj_field_int64_get(npc,OBJ_F_LOCATION);
        unsigned flags=obj_field_int32_get(npc,OBJ_F_FLAGS);
        if(flags&(OF_DESTROYED|OF_EXTINCT|OF_OFF))e->state=COMPANION_UNAVAILABLE;
        else if(critter_is_dead(npc))e->state=COMPANION_DEAD;
        else if(ours)e->state=(obj_field_int32_get(npc,OBJ_F_NPC_FLAGS)&ONF_AI_WAIT_HERE)?COMPANION_WAITING:COMPANION_PRESENT;
        else if(e->state==COMPANION_PRESENT || e->state==COMPANION_WAITING)e->state=COMPANION_LEFT;
        if(index<0)e->recovered=1;
    } while(obj_inst_next(&npc,&iter));
}
bool modern_companions_save(void)
{
    modern_companions_refresh();
    if(!companion_loaded)return true;
    TigFile* f=tig_file_fopen(COMPANION_FILE,"wb");if(!f)return false;
    uint32_t header[]={0x434A5241,1,(uint32_t)companion_count};
    bool ok=tig_file_fwrite(header,4,3,f)==3 && tig_file_fwrite(&companion_owner,24,1,f)==1;
    for(int i=0;ok && i<companion_count;i++) {
        CompanionEntry* e=&companion_entries[i];int32_t values[]={e->description,e->map,e->state,e->recovered};
        ok=tig_file_fwrite(&e->id,24,1,f)==1 && tig_file_fwrite(values,4,4,f)==4
            && tig_file_fwrite(&e->location,8,1,f)==1;
    }
    return tig_file_fclose(f)==0 && ok;
}
int modern_companions_count(void){return companion_count;}
const CompanionEntry* modern_companions_get(int index)
{
    return index>=0 && index<companion_count?&companion_entries[index]:NULL;
}
