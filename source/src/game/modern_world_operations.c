#include "game/modern_world_updates_internal.h"
#include "game/gamelib.h"
#include "game/map.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/object.h"
#include "game/location.h"
#include "game/light.h"
#include "game/script.h"
#include "game/quest.h"
#include "game/obj_file.h"
#include "game/item.h"
#include "game/player.h"
#include "game/wallcheck.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Provenance belongs to the actual opened SEC stream, not its mounted name. */
typedef struct RwSource { char map[RW_ID_SIZE],path[RW_PATH_SIZE]; int64_t id; uint8_t hash[32]; bool queued; } RwSource;
static RwSource* sources;
static unsigned source_count;
static bool failed;
static RwPackagePrepare prepare_observer;
static RwAddObserver add_observer;
static void* observer_context;
bool rw_operations_healthy(void){return !failed;}
void rw_operations_set_observers(RwPackagePrepare prepare,RwAddObserver added,void* context)
{
    prepare_observer=prepare;add_observer=added;observer_context=context;
}

static const char* current_map(void)
{
    static char normalized[RW_ID_SIZE];
    char* name=NULL;
    normalized[0]=0;
    if(map_get_name(map_current_map(),&name)&&name){snprintf(normalized,sizeof(normalized),"%s",name);for(char* p=normalized;*p;p++)*p=(char)tolower((unsigned char)*p);}
    return normalized;
}
static ObjectID guid(const char* text)
{
    ObjectID id={0}; unsigned char* raw=(unsigned char*)&id;
    if(strlen(text)!=36)return id;
    for(unsigned i=0;i<36;i++)if(!isxdigit((unsigned char)text[i]))return id;
    for(unsigned i=0;i<18;i++){unsigned b;sscanf(text+i*2,"%2x",&b);raw[i<2?i:i+6]=(unsigned char)b;}
    if(id.type!=OID_TYPE_GUID)return (ObjectID){0};
    return id;
}
static int scalar_field(const RwOperation* op)
{
    if(!strcmp(op->field,"DESCRIPTION"))return OBJ_F_DESCRIPTION;
    if(!strcmp(op->field,"NAME"))return OBJ_F_NAME;
    if(!strcmp(op->field,"SOUND_EFFECT"))return OBJ_F_SOUND_EFFECT;
    if(op->type==OBJ_TYPE_NPC && !strcmp(op->field,"CRITTER_DESCRIPTION_UNKNOWN"))return OBJ_F_CRITTER_DESCRIPTION_UNKNOWN;
    if(op->type==OBJ_TYPE_PORTAL){
        if(!strcmp(op->field,"PORTAL_LOCK_DIFFICULTY"))return OBJ_F_PORTAL_LOCK_DIFFICULTY;
        if(!strcmp(op->field,"PORTAL_KEY_ID"))return OBJ_F_PORTAL_KEY_ID;
    }
    if(op->type==OBJ_TYPE_CONTAINER){
        if(!strcmp(op->field,"CONTAINER_LOCK_DIFFICULTY"))return OBJ_F_CONTAINER_LOCK_DIFFICULTY;
        if(!strcmp(op->field,"CONTAINER_KEY_ID"))return OBJ_F_CONTAINER_KEY_ID;
    }
    return -1;
}
static bool loc_valid(uint64_t loc)
{
    return location_get_x(loc)<128000 && location_get_y(loc)<128000;
}
bool rw_operation_validate(const RwOperation* op,char* error,size_t size)
{
    const char* why=NULL;
    bool cell=op->kind==RW_GROUND||op->kind==RW_ROOF||op->kind==RW_BLOCK;
    uint8_t zero[32]={0};
    if(op->kind<RW_ADD||op->kind>RW_GLOBAL||op->policy>RW_AUTHORITATIVE)why="Unsupported operation kind or policy";
    else if(op->kind==RW_GLOBAL){
        if(op->selector!=RW_NONE)why="Global operation cannot select an object";
        else if(!strcmp(op->field,"global_var")){if(op->index>=2000||op->before[0]>UINT32_MAX||op->after[0]>UINT32_MAX)why="Invalid global variable";}
        else if(!strcmp(op->field,"global_flag")){if(op->index>=3200||op->before[0]>1||op->after[0]>1)why="Invalid global flag";}
        else if(!strcmp(op->field,"quest_state")){
            if(op->policy==RW_AUTHORITATIVE)why="Quest conversion requires a conditional guard";
            else if(op->index<1000||op->index>=2000||op->before[0]>6||op->after[0]>6)why="Invalid local-PC quest conversion";
            else if(op->before[0]>=4&&op->before[0]!=op->after[0])why="Terminal quest state cannot be reset";
        }else why="Global field is not allowlisted";
    }else if(cell){
        if(op->selector!=RW_NONE||op->index>=(op->kind==RW_ROOF?256u:4096u))why="Invalid cell selector";
        else if(op->before[0]>UINT32_MAX||op->after[0]>UINT32_MAX||(op->kind==RW_BLOCK&&(op->before[0]>1||op->after[0]>1)))why="Invalid cell value";
    }else if(op->type>17||op->type==15||op->prototype==0)why="Invalid object type or prototype";
    else if(op->selector!=RW_GUID&&op->selector!=RW_STATIC)why="Object operation needs an identity";
    else if(op->selector==RW_GUID&&guid(op->guid).type!=OID_TYPE_GUID)why="Invalid persistent GUID";
    else if(op->kind==RW_ADD){if(op->selector!=RW_GUID||op->type==4||!op->payload_path[0])why="Add requires a supported GUID MOB payload";}
    else if(op->kind==RW_MOVE){
        if(op->selector!=RW_GUID||(op->type!=2&&op->type!=3))why="Only GUID containers and scenery can move";
        else if(!loc_valid(op->before[0])||!loc_valid(op->after[0])||sector_id_from_loc(op->before[0])!=(int64_t)op->sector||sector_id_from_loc(op->after[0])!=(int64_t)op->sector)why="Move must remain in its original sector";
        else if(op->before[1]>UINT32_MAX||op->before[2]>UINT32_MAX||op->after[1]>UINT32_MAX||op->after[2]>UINT32_MAX)why="Invalid move offsets";
    }else if(op->kind==RW_FIELD){if(scalar_field(op)<0||op->before[0]>UINT32_MAX||op->after[0]>UINT32_MAX)why="Scalar field is not allowlisted or exceeds 32 bits";}
    else if(op->kind==RW_FLAGS){if(strcmp(op->field,"FLAGS")||!op->mask||(op->mask&~UINT64_C(0xD02))||(op->type>3&&op->type!=17)||(op->before[0]&~op->mask)||(op->after[0]&~op->mask))why="Invalid flags mask, type or guarded value";}
    else if(op->kind==RW_HIDE){if(op->type!=3||(op->before[0]!=0&&op->before[0]!=OF_OFF)||op->after[0]!=OF_OFF)why="Hide requires scenery and an OFF-bit guard";}
    if(!why&&(cell||op->selector==RW_STATIC)&&(!op->base_path[0]||!memcmp(op->base_hash,zero,32)))why="Base SEC digest is required";
    if(why){snprintf(error,size,"%s",why);return false;}
    return true;
}
static bool record(const RwPackage* p,const RwOperation* op,unsigned outcome,const char* detail)
{
    if(!rw_state_record(p,op,outcome,detail)){failed=true;rw_error("Cannot record outcome for %s/%s",p->id,op->id);return false;}
    return true;
}
static RwSource* source_for(const char* map,int64_t id)
{
    for(unsigned i=0;i<source_count;i++)if(sources[i].id==id&&!SDL_strcasecmp(sources[i].map,map))return &sources[i];
    return NULL;
}
void rw_operations_reset(void){free(sources);sources=NULL;source_count=0;failed=false;}
void rw_operations_source_digest(int64_t id,const char* path,const uint8_t digest[32])
{
    const char* map=current_map(); bool relevant=false;
    for(unsigned p=0;p<rw_package_count&&!relevant;p++)for(unsigned j=0;j<rw_packages[p].operation_count;j++){
        RwOperation* op=&rw_packages[p].operations[j];
        if(op->kind!=RW_GLOBAL&&op->sector==(uint64_t)id&&!SDL_strcasecmp(op->map,map)){relevant=true;break;}
    }
    if(!relevant)return;
    RwSource* src=source_for(map,id);
    if(!src){
        RwSource* next=realloc(sources,(source_count+1)*sizeof(*sources));
        if(!next){failed=true;rw_error("Cannot retain SEC provenance");return;}
        sources=next;src=&sources[source_count++];memset(src,0,sizeof(*src));src->id=id;snprintf(src->map,sizeof(src->map),"%s",map);
    }
    snprintf(src->path,sizeof(src->path),"%s",path);memcpy(src->hash,digest,32);
}
void rw_operations_source(int64_t id,const char* path)
{
    uint8_t digest[32];if(rw_hash_resource(path,digest))rw_operations_source_digest(id,path,digest);
}
static bool identity(int64_t obj,const RwOperation* op)
{
    if(!obj||obj_is_proto(obj)||obj_field_int32_get(obj,OBJ_F_TYPE)!=(int)op->type)return false;
    ObjectID id=sub_407100(obj,OBJ_F_PROTOTYPE_HANDLE);
    return id.type==OID_TYPE_A&&(unsigned)id.d.a==op->prototype;
}
static int64_t resolve(Sector* sector,const RwOperation* op)
{
    if(op->selector==RW_GUID)return obj_pool_perm_lookup(guid(op->guid));
    for(unsigned i=0;i<4096;i++)for(ObjectNode* n=sector->objects.heads[i];n;n=n->next)
        if(obj_field_int32_get(n->obj,OBJ_F_TEMP_ID)==(int)op->ordinal&&object_is_static(n->obj))return n->obj;
    return 0;
}
static uint32_t roof_value(uint32_t v){return tig_art_roof_id_fade_set(tig_art_roof_id_fill_set(v,0),0);}
void rw_operations_capture(Sector* sector)
{
    if(!rw_active||failed)return;
    for(unsigned i=0;i<256;i++){uint32_t v;if(rw_state_roof_get(current_map(),sector->id,i,&v))
        if(!rw_state_roof_put(current_map(),sector->id,i,roof_value(sector->roofs.art_ids[i])))failed=true;}
}
void rw_scene_hide(int64_t obj)
{
    object_flags_set(obj,OF_OFF);light_set_flags(obj,LF_OFF);shadow_clear(obj);
}
void rw_scene_ground(Sector* sector,unsigned index,uint32_t value)
{
    if(index>=4096)return;
    sector->tiles.art_ids[index]=value;sector->tiles.difmask[index/32]|=1u<<(index%32);sector->tiles.dif=1;
    object_invalidate_rect(NULL);
}
void rw_scene_roof(Sector* sector,unsigned index,uint32_t value)
{
    if(index>=256)return;
    sector->roofs.art_ids[index]=value;sector->roofs.empty=0;
    if(player_get_local_pc_obj())wallcheck_recalc(obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION));
    object_invalidate_rect(NULL);
}
void rw_scene_block(Sector* sector,unsigned index,bool blocked)
{
    if(index>=4096)return;
    uint32_t mask=1u<<(index%32);sector->blocks.mask[index/32]=(sector->blocks.mask[index/32]&~mask)|(blocked?mask:0);sector->blocks.modified=true;
    object_invalidate_rect(NULL);
}
static bool guards(const RwOperation* op,const uint64_t* values,unsigned count)
{
    return op->policy==RW_AUTHORITATIVE||!memcmp(values,op->before,count*sizeof(*values));
}
static void apply_one(const RwPackage* p,const RwOperation* op,Sector* s,RwSource* source)
{
    int status=rw_state_outcome(p,op);if(status==RW_APPLIED)return;
    if(status<0){failed=true;rw_error("Operation identity changed: %s/%s",p->id,op->id);return;}
    bool cell=op->kind==RW_GROUND||op->kind==RW_ROOF||op->kind==RW_BLOCK;
    if((cell||op->selector==RW_STATIC)&&(!source||memcmp(source->hash,op->base_hash,32))){record(p,op,RW_CONFLICT,"Base SEC digest differs");return;}
    uint64_t value[3]={0};unsigned count=1;int64_t obj=0;
    if(cell){
        if(op->kind==RW_GROUND)value[0]=s->tiles.art_ids[op->index];
        else if(op->kind==RW_ROOF)value[0]=roof_value(s->roofs.art_ids[op->index]);
        else value[0]=(s->blocks.mask[op->index/32]>>(op->index%32))&1;
    }else{
        obj=resolve(s,op);
        if(!obj){record(p,op,RW_CONFLICT,"Target is absent; current player state preserved");return;}
        if(!identity(obj,op)){record(p,op,RW_CONFLICT,"Target type or prototype differs");return;}
        if(sector_id_from_loc(obj_field_int64_get(obj,OBJ_F_LOCATION))!=(int64_t)op->sector){record(p,op,RW_CONFLICT,"Target has moved out of the authored sector");return;}
        if(op->kind==RW_MOVE){
            if(object_is_static(obj)){record(p,op,RW_CONFLICT,"Target is static; moving it would reorder DIF records");return;}
            value[0]=obj_field_int64_get(obj,OBJ_F_LOCATION);value[1]=(uint32_t)obj_field_int32_get(obj,OBJ_F_OFFSET_X);value[2]=(uint32_t)obj_field_int32_get(obj,OBJ_F_OFFSET_Y);count=3;
        }else if(op->kind==RW_FIELD)value[0]=(uint32_t)obj_field_int32_get(obj,scalar_field(op));
        else value[0]=(uint32_t)obj_field_int32_get(obj,OBJ_F_FLAGS)&(op->kind==RW_HIDE?OF_OFF:op->mask);
    }
    if(memcmp(value,op->after,count*sizeof(*value))){
        if(!guards(op,value,count)){record(p,op,RW_CONFLICT,"Guarded value differs; player state preserved");return;}
        switch(op->kind){
        case RW_MOVE:sub_43E770(obj,op->after[0],(int32_t)op->after[1],(int32_t)op->after[2]);break;
        case RW_FIELD:obj_field_int32_set(obj,scalar_field(op),(int32_t)op->after[0]);break;
        case RW_FLAGS:object_flags_unset(obj,(unsigned)(op->mask&~op->after[0]));object_flags_set(obj,(unsigned)op->after[0]);break;
        case RW_GROUND:rw_scene_ground(s,op->index,(uint32_t)op->after[0]);break;
        case RW_ROOF:rw_scene_roof(s,op->index,(uint32_t)op->after[0]);break;
        case RW_BLOCK:rw_scene_block(s,op->index,op->after[0]!=0);break;
        }
    }
    if(op->kind==RW_HIDE)rw_scene_hide(obj);
    if(op->kind==RW_ROOF&&!rw_state_roof_put(current_map(),op->sector,op->index,(uint32_t)op->after[0])){failed=true;return;}
    object_invalidate_rect(NULL);record(p,op,RW_APPLIED,"Guarded operation applied");
}

typedef struct RwPending {const RwOperation* op;int64_t obj;bool created;} RwPending;
static int pending_index(RwPending* entries,unsigned count,int64_t obj)
{
    for(unsigned i=0;i<count;i++)if(entries[i].obj==obj&&entries[i].created)return (int)i;
    return -1;
}
static void add_batch(const RwPackage* p,Sector* sector)
{
    RwPending* entries=calloc(p->operation_count?p->operation_count:1,sizeof(*entries));
    if(!entries){failed=true;rw_error("Cannot allocate object installation batch");return;}
    unsigned count=0;const char* error=NULL;
    for(unsigned i=0;i<p->operation_count;i++){
        const RwOperation* op=&p->operations[i];
        if(op->kind!=RW_ADD||op->sector!=(uint64_t)sector->id||SDL_strcasecmp(op->map,current_map()))continue;
        int status=rw_state_outcome(p,op);
        if(status==RW_APPLIED)continue;
        if(status<0){error="Operation identity changed";failed=true;break;}
        RwPending* entry=&entries[count++];entry->op=op;entry->obj=obj_pool_perm_lookup(guid(op->guid));
        if(entry->obj){if(!identity(entry->obj,op)){error="Existing GUID has a different type or prototype";break;}continue;}
        TigFile* stream=tig_file_fopen_repository(p->directory,op->payload_path);
        uint8_t hash[32];const uint8_t* expected=NULL;
        for(unsigned r=0;r<p->resource_count;r++)if(!strcmp(p->resources[r].path,op->payload_path)){expected=p->resources[r].digest;break;}
        if(!stream||!expected||!rw_hash_stream(stream,hash)||memcmp(hash,expected,32)){if(stream)tig_file_fclose(stream);error="MOB payload digest differs";break;}
        /* Inspect identity before obj_read can register it in the pool. A MOB
         * starts with version, prototype OID, instance OID. */
        uint32_t version,type;ObjectID proto,oid;
        bool header=stream&&tig_file_fread(&version,4,1,stream)==1&&tig_file_fread(&proto,24,1,stream)==1&&tig_file_fread(&oid,24,1,stream)==1&&tig_file_fread(&type,4,1,stream)==1;
        if(!header||type!=op->type||!objid_is_equal(oid,guid(op->guid))||proto.type!=OID_TYPE_A||(unsigned)proto.d.a!=op->prototype){tig_file_fclose(stream);error="MOB identity differs from operation";break;}
        int64_t prototype=obj_pool_perm_lookup(proto);
        if(!prototype||!obj_is_proto(prototype)||obj_field_int32_get(prototype,OBJ_F_TYPE)!=(int)op->type){tig_file_fclose(stream);error="MOB prototype is absent or has a different native type";break;}
        entry->created=true;
        bool read=tig_file_fseek(stream,0,SEEK_SET)==0&&obj_read(stream,&entry->obj);tig_file_fclose(stream);
        if(!read||!entry->obj){error="MOB could not be read";break;}
        if(!identity(entry->obj,op)||!loc_valid(obj_field_int64_get(entry->obj,OBJ_F_LOCATION))||sector_id_from_loc(obj_field_int64_get(entry->obj,OBJ_F_LOCATION))!=sector->id){error="MOB type or location differs from operation";break;}
        unsigned flags=(unsigned)obj_field_int32_get(entry->obj,OBJ_F_FLAGS);
        if(flags&(OF_DESTROYED|OF_EXTINCT)){error="MOB is already destroyed or extinct";break;}
    }
    if(!error){
        /* Resolve only after every new GUID exists. No existing object is
         * postprocessed or rewritten, including an owner already installed. */
        for(unsigned i=0;i<count;i++)if(entries[i].created)obj_load_postprocess(entries[i].obj);
        for(unsigned i=0;i<count;i++)if(entries[i].created&&add_observer)add_observer(p,entries[i].op,entries[i].obj,true,observer_context);
        for(unsigned i=0;i<count&&!error;i++)if(entries[i].created){
            int64_t obj=entries[i].obj;int type=obj_field_int32_get(obj,OBJ_F_TYPE);
            if(type>=OBJ_TYPE_WEAPON&&type<=OBJ_TYPE_GENERIC){
                int64_t parent=obj_field_handle_get(obj,OBJ_F_ITEM_PARENT);
                if(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_INVENTORY){
                    if(!parent||pending_index(entries,count,parent)<0){error="Inventory owner is not part of this new batch";break;}
                    int pt=obj_field_int32_get(parent,OBJ_F_TYPE),list=pt==OBJ_TYPE_CONTAINER?OBJ_F_CONTAINER_INVENTORY_LIST_IDX:OBJ_F_CRITTER_INVENTORY_LIST_IDX;
                    if(pt!=OBJ_TYPE_CONTAINER&&pt!=OBJ_TYPE_NPC){error="Invalid inventory owner type";break;}
                    bool found=false;int n=obj_arrayfield_length_get(parent,list);
                    for(int k=0;k<n;k++)if(obj_arrayfield_handle_get(parent,list,k)==obj)found=true;
                    if(!found){error="Owner does not reference its inventory item";break;}
                }else if(parent){error="World item has an inventory parent";break;}
            }
            if(type==OBJ_TYPE_CONTAINER||type==OBJ_TYPE_NPC){
                int list=type==OBJ_TYPE_CONTAINER?OBJ_F_CONTAINER_INVENTORY_LIST_IDX:OBJ_F_CRITTER_INVENTORY_LIST_IDX;
                int num=type==OBJ_TYPE_CONTAINER?OBJ_F_CONTAINER_INVENTORY_NUM:OBJ_F_CRITTER_INVENTORY_NUM;
                int n=obj_field_int32_get(obj,num);
                if(n<0||n>8192||n!=obj_arrayfield_length_get(obj,list)){error="Inventory count differs from its list";break;}
                for(int k=0;k<n;k++){
                    int64_t child=obj_arrayfield_handle_get(obj,list,k);
                    int child_type=child?obj_field_int32_get(child,OBJ_F_TYPE):-1;
                    if(!child||child_type<OBJ_TYPE_WEAPON||child_type>OBJ_TYPE_GENERIC||pending_index(entries,count,child)<0||!(obj_field_int32_get(child,OBJ_F_FLAGS)&OF_INVENTORY)||obj_field_handle_get(child,OBJ_F_ITEM_PARENT)!=obj){error="Inventory reference is absent or belongs to existing player state";break;}
                    for(int earlier=0;earlier<k;earlier++)if(obj_arrayfield_handle_get(obj,list,earlier)==child){error="Inventory list repeats an item";break;}
                }
            }
        }
    }
    if(error){
        for(unsigned i=0;i<count;i++)if(entries[i].created&&entries[i].obj)obj_deallocate(entries[i].obj);
        for(unsigned i=0;i<count;i++)record(p,entries[i].op,RW_CONFLICT,error);
        free(entries);return;
    }
    for(unsigned i=0;i<count;i++){
        if(!entries[i].created&&add_observer)add_observer(p,entries[i].op,entries[i].obj,false,observer_context);
        if(entries[i].created)object_install_loaded(entries[i].obj);
    }
    for(unsigned i=0;i<count;i++)if(entries[i].created){
        int64_t obj=entries[i].obj;int type=obj_field_int32_get(obj,OBJ_F_TYPE);
        if(type>=OBJ_TYPE_WEAPON&&type<=OBJ_TYPE_GENERIC&&(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_INVENTORY)){
            int slot=obj_field_int32_get(obj,OBJ_F_ITEM_INV_LOCATION);if(slot>=1000&&slot<=1008)item_location_set(obj,slot);
        }
    }
    for(unsigned i=0;i<count;i++)record(p,entries[i].op,RW_APPLIED,entries[i].created?"New GUID installed once":"Existing GUID adopted without rewriting state");
    free(entries);
}
static void apply_sector(Sector* sector)
{
    const char* map=current_map();
    RwSource snapshot;RwSource* source=source_for(map,sector->id);
    if(source){snapshot=*source;source=&snapshot;}
    for(unsigned i=0;i<256;i++){uint32_t value;if(rw_state_roof_get(map,sector->id,i,&value)){sector->roofs.art_ids[i]=value;sector->roofs.empty=0;}}
    for(unsigned i=0;i<rw_package_count&&!failed;i++){
        const RwPackage* p=&rw_packages[i];
        if(prepare_observer&&!prepare_observer(p,sector,observer_context)){failed=true;rw_error("Package preparation failed: %s",p->id);break;}
        add_batch(p,sector);
        for(unsigned j=0;j<p->operation_count&&!failed;j++){
            const RwOperation* op=&p->operations[j];
            if(op->kind!=RW_ADD&&op->kind!=RW_GLOBAL&&op->sector==(uint64_t)sector->id&&!SDL_strcasecmp(op->map,map))apply_one(p,op,sector,source);
        }
    }
}
void rw_operations_sector(int64_t id,Sector* sector)
{
    if(rw_applying||failed)return;
    RwSource* src=source_for(current_map(),id);
    if(!src){uint8_t empty[32]={0};rw_operations_source_digest(id,"",empty);src=source_for(current_map(),id);}
    if(!src)return;
    if(gamelib_in_load()||!rw_active){src->queued=true;return;}
    rw_applying=true;apply_sector(sector);rw_applying=false;
}
bool rw_operations_finish_load(void)
{
    if(failed)return false;
    rw_applying=true;
    for(unsigned i=0;i<source_count;i++)if(sources[i].queued&&!SDL_strcasecmp(sources[i].map,current_map())){
        int64_t id=sources[i].id;sources[i].queued=false;
        if(sector_loaded(id)){Sector* sector;if(!sector_lock(id,&sector)){failed=true;break;}apply_sector(sector);sector_unlock(id);}
    }
    rw_applying=false;return !failed;
}
bool rw_operations_global(void)
{
    for(unsigned i=0;i<rw_package_count;i++)for(unsigned j=0;j<rw_packages[i].operation_count;j++){
        const RwPackage* p=&rw_packages[i];const RwOperation* op=&p->operations[j];
        if(op->kind!=RW_GLOBAL)continue;
        int status=rw_state_outcome(p,op);if(status==RW_APPLIED)continue;if(status<0){failed=true;return false;}
        if(!strcmp(op->field,"quest_state")){
            char detail[256];
            if(!quest_state_migrate(player_get_local_pc_obj(),(int)op->index,(int)op->before[0],(int)op->after[0],op->policy==RW_AUTHORITATIVE,detail,sizeof(detail)))record(p,op,RW_CONFLICT,detail);
            else record(p,op,RW_APPLIED,"Local-PC quest state migrated without rewards or side effects");
            continue;
        }
        uint64_t value=!strcmp(op->field,"global_var")?(uint32_t)script_global_var_get(op->index):(uint32_t)script_global_flag_get(op->index);
        if(value!=op->after[0]){
            if(!guards(op,&value,1)){record(p,op,RW_CONFLICT,"Global guard differs");continue;}
            if(!strcmp(op->field,"global_var"))script_global_var_set(op->index,(int32_t)op->after[0]);
            else script_global_flag_set(op->index,(int)op->after[0]);
        }
        record(p,op,RW_APPLIED,"Guarded global conversion applied");
    }
    return !failed;
}
