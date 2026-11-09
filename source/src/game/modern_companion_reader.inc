/* Strict, allocation-free v119 metadata reader. No object handles are created. */
static TigFile* companion_stream;
static CompanionSnapshot* companion_snapshot;
static int companion_fields_read;
static bool companion_skip(uint64_t n)
{
    long pos=tig_file_ftell(companion_stream);
    int end=tig_file_filelength(companion_stream);
    return pos>=0 && end>=pos && n<=(uint64_t)(end-pos)
        && tig_file_fseek(companion_stream,(long)n,SEEK_CUR)==0;
}
static bool companion_field(Object* object,int index,ObjectFieldInfo* field)
{
    (void)object;(void)index;
    companion_fields_read++;
    int id=(int)(field-object_fields),v=0;
    uint8_t present=0;
    if(field->type==OD_TYPE_INT32) {
        if(tig_file_fread(&v,4,1,companion_stream)!=1)return false;
        if(id==OBJ_F_DESCRIPTION)companion_snapshot->description=v;
        if(id==OBJ_F_NPC_FLAGS)companion_snapshot->flags=v;
        if(id==OBJ_F_FLAGS)companion_snapshot->object_flags=v;
        return true;
    }
    if(tig_file_fread(&present,1,1,companion_stream)!=1 || present>1)return false;
    if(!present) {
        if(id==OBJ_F_LOCATION)companion_snapshot->location=0;
        if(id==OBJ_F_NPC_LEADER)memset(&companion_snapshot->leader,0,sizeof(ObjectID));
        return true;
    }
    if(field->type==OD_TYPE_INT64) {
        int64_t value;
        if(tig_file_fread(&value,8,1,companion_stream)!=1)return false;
        if(id==OBJ_F_LOCATION)companion_snapshot->location=value;
        return true;
    }
    if(field->type==OD_TYPE_HANDLE) {
        ObjectID value;
        if(tig_file_fread(&value,sizeof(value),1,companion_stream)!=1)return false;
        if(id==OBJ_F_NPC_LEADER)companion_snapshot->leader=value;
        return true;
    }
    if(field->type==OD_TYPE_STRING) {
        uint32_t length;
        return tig_file_fread(&length,4,1,companion_stream)==1 && length<1048576 && companion_skip((uint64_t)length+1);
    }
    if((field->type>=OD_TYPE_INT32_ARRAY && field->type<=OD_TYPE_QUEST_ARRAY) || field->type==OD_TYPE_HANDLE_ARRAY) {
        uint32_t header[3],words;
        unsigned size=field->type==OD_TYPE_HANDLE_ARRAY?24:field->type==OD_TYPE_SCRIPT_ARRAY?sizeof(Script):
            field->type==OD_TYPE_QUEST_ARRAY?sizeof(PcQuestState):
            (field->type==OD_TYPE_INT64_ARRAY || field->type==OD_TYPE_UINT64_ARRAY)?8:4;
        if(tig_file_fread(header,4,3,companion_stream)!=3 || header[0]!=size || header[1]>100000)return false;
        if(!companion_skip((uint64_t)header[0]*header[1]))return false;
        return tig_file_fread(&words,4,1,companion_stream)==1 && words<=100000 && companion_skip((uint64_t)words*4);
    }
    return false;
}
bool obj_companion_snapshot(TigFile* stream,CompanionSnapshot* snapshot)
{
    Object object={0};int version;int masks[32]={0};
    memset(snapshot,0,sizeof(*snapshot));
    companion_stream=stream;companion_snapshot=snapshot;companion_fields_read=0;
    if(tig_file_fread(&version,4,1,stream)!=1 || version!=119
        || tig_file_fread(&object.prototype_oid,24,1,stream)!=1
        || object.prototype_oid.type!=OID_TYPE_A
        || tig_file_fread(&object.oid,24,1,stream)!=1
        || tig_file_fread(&object.type,4,1,stream)!=1 || object.type<0 || object.type>OBJ_TYPE_TRAP
        || tig_file_fread(&object.num_fields,2,1,stream)!=1 || object.num_fields<0 || object.num_fields>512)return false;
    int count=sub_40C030(object.type);
    if(count<0 || count>32 || tig_file_fread(masks,4,count,stream)!=(size_t)count)return false;
    object.field_48=masks;
    int64_t prototype=obj_pool_perm_lookup(object.prototype_oid);
    if(prototype!=OBJ_HANDLE_NULL && obj_is_proto(prototype)) {
        snapshot->description=obj_field_int32_get(prototype,OBJ_F_DESCRIPTION);
        snapshot->object_flags=obj_field_int32_get(prototype,OBJ_F_FLAGS);
        if(object.type==OBJ_TYPE_NPC)snapshot->flags=obj_field_int32_get(prototype,OBJ_F_NPC_FLAGS);
    }
    if(!object_inst_enumerate_overridden_fields(&object,companion_field))return false;
    int bits=0;for(int i=0;i<count;i++)bits+=bitset_count_bits(masks[i],32);
    if(companion_fields_read!=object.num_fields || bits!=companion_fields_read)return false;
    snapshot->id=object.oid;snapshot->type=object.type;
    return true;
}
bool obj_companion_diff(TigFile* stream,CompanionSnapshot* snapshot)
{
    Object object={0};int version,masks[32]={0};unsigned marker;ObjectID id;
    companion_stream=stream;companion_snapshot=snapshot;companion_fields_read=0;
    if(snapshot->type<0 || snapshot->type>OBJ_TYPE_TRAP)return false;
    object.type=snapshot->type;object.field_48=masks;
    int count=sub_40C030(object.type);
    if(count<0 || count>32 || tig_file_fread(&version,4,1,stream)!=1 || version!=119
        || tig_file_fread(&marker,4,1,stream)!=1 || marker!=0x12344321
        || tig_file_fread(&id,24,1,stream)!=1 || !objid_is_equal(id,snapshot->id)
        || tig_file_fread(masks,4,count,stream)!=(size_t)count)return false;
    if(!object_inst_enumerate_overridden_fields(&object,companion_field))return false;
    int bits=0;for(int i=0;i<count;i++)bits+=bitset_count_bits(masks[i],32);
    return bits==companion_fields_read && tig_file_fread(&marker,4,1,stream)==1 && marker==0x23455432;
}
