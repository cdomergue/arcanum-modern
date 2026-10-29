/* Additive estate content. Stable object GUIDs and ownership globals are save ABI.
 * Never replace a base SEC: its object ordering belongs to existing save DIFs. */
#include "game/modern_house.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/mes.h"
#include "game/map.h"
#include "game/location.h"
#include "game/light.h"
#include "game/obj.h"
#include "game/obj_flags.h"
#include "game/obj_pool.h"
#include "game/obj_file.h"
#include "game/object.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/descriptions.h"
#include "game/wallcheck.h"
#include "game/item.h"
#include "game/sector.h"
#include "game/tile.h"
#include "ui/dialog_ui.h"

#define ESTATE_AGENT 48301
#define ESTATE_GUARD 48300
#define ESTATE_BED 48304
static const char seller_id[] = "0200e1a754aef2a44efaa58c8bf5498a21b9";
static_assert(sizeof(seller_id)==37, "Seller ID must encode 18 bytes");
static int64_t seller_handle;
static int seller_x=61991,seller_y=65620,seller_dx,seller_dy,seller_rotation=3;
typedef struct Property {
    int slot, price, global, discovered, key, x, y;
    char scene[64];
    mes_file_handle_t objects, terrain, roofs, removed, vegetation;
} Property;
static Property properties[100];
static int property_count;
static mes_file_handle_t descriptions = MES_FILE_HANDLE_INVALID;
static bool ready;

static ObjectID guid(const char* str)
{
    ObjectID id = {0}; unsigned char* raw = (unsigned char*)&id;
    if (strlen(str) != 36) return id;
    for (int i = 0; i < 18; i++) { unsigned b; if (sscanf(str+i*2, "%2x", &b)!=1) return (ObjectID){0}; raw[i<2?i:i+6]=(unsigned char)b; }
    return id;
}
static int64_t lookup(const char* id) { return obj_pool_perm_lookup(guid(id)); }
static bool owned(Property* p) { return script_global_var_get(p->global) == 1; }
static void attach(int64_t obj, int event, int number)
{
    Script previous = {0}; obj_arrayfield_script_get(obj, OBJ_F_SCRIPTS_IDX, event, &previous);
    if (previous.num == number) return;
    Script scr = {0}; scr.num = number;
    if (number) script_load_hdr(&scr);
    obj_arrayfield_script_set(obj, OBJ_F_SCRIPTS_IDX, event, &scr);
}
static bool is_seller(int64_t obj)
{
    if(!obj || !obj_handle_is_valid(obj) || obj_is_proto(obj)
      || obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_NPC
      || obj_field_int32_get(obj,OBJ_F_DESCRIPTION)!=61002)return false;
    Script scr={0};obj_arrayfield_script_get(obj,OBJ_F_SCRIPTS_IDX,SAP_DIALOG,&scr);
    return scr.num==ESTATE_AGENT && !(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_DESTROYED);
}
static int64_t find_seller(void)
{
    if(is_seller(seller_handle))return seller_handle;
    seller_handle=lookup(seller_id);
    if(!is_seller(seller_handle))seller_handle=OBJ_HANDLE_NULL;
    /* Early test saves contain random IDs. Retain one tagged agent and retire
     * only our duplicate agents; their inventories remain in the save. */
    int64_t obj;int iter;
    if(obj_inst_first(&obj,&iter))do{
        if(!is_seller(obj))continue;
        if(!seller_handle)seller_handle=obj;
        else if(obj!=seller_handle){
            attach(obj,SAP_DIALOG,0);
            object_flags_set(obj,OF_OFF|OF_DONTDRAW);
        }
    }while(obj_inst_next(&obj,&iter));
    if(seller_handle){
        obj_field_int32_set(seller_handle,OBJ_F_CRITTER_DESCRIPTION_UNKNOWN,61005);
        int64_t loc=location_make(seller_x,seller_y);
        if(obj_field_int64_get(seller_handle,OBJ_F_NPC_STANDPOINT_DAY)!=loc){
            obj_field_int64_set(seller_handle,OBJ_F_NPC_STANDPOINT_DAY,loc);
            obj_field_int64_set(seller_handle,OBJ_F_NPC_STANDPOINT_NIGHT,loc);
            obj_field_int64_set(seller_handle,OBJ_F_CRITTER_TELEPORT_DEST,loc);
            sub_43E770(seller_handle,loc,seller_dx,seller_dy);
            object_set_current_aid(seller_handle,tig_art_id_rotation_set(obj_field_int32_get(seller_handle,OBJ_F_CURRENT_AID),seller_rotation));
        }
    }
    return seller_handle;
}
static bool on_main_map(void)
{
    const char* module = gamelib_current_module_name_get(); char* name = NULL;
    return ready && module && !SDL_strcasecmp(module,"Arcanum")
        && player_get_local_pc_obj() != OBJ_HANDLE_NULL
        && map_get_name(map_current_map(), &name) && name && !SDL_strcasecmp(name,"Arcanum1-024-fixed");
}
void modern_house_load(void)
{
    /* An absent content pack is optional; do not change unmodified installations. */
    FILE* f = fopen("ArcanumModern/house/base/house/catalog.mes", "rb");
    if (!f) return;
    fclose(f);
    if (!tig_file_repository_add("ArcanumModern/house/base")) return;
    const char* lang = getenv("ARCANUM_HOUSE_LANGUAGE");
    bool french = lang && !strcmp(lang,"fr");
    if (!lang) { f=fopen("modern-ui/uap-fr/active/ready","rb"); if(f){french=true;fclose(f);} }
    if (!tig_file_repository_add(french ? "ArcanumModern/house/fr" : "ArcanumModern/house/en")) return;
    mes_file_handle_t placement;
    if(mes_load("house/seller.mes",&placement)){
        MesFileEntry e={.num=0};int x,y,dx,dy,rotation;
        if(mes_search(placement,&e) && sscanf(e.str,"%d %d %d %d %d",&x,&y,&dx,&dy,&rotation)==5
           && x>=0 && y>=0 && x<128000 && y<128000 && rotation>=0 && rotation<8){
            seller_x=x;seller_y=y;seller_dx=dx;seller_dy=dy;seller_rotation=rotation;
        }
        mes_unload(placement);
    }
    mes_file_handle_t catalog;
    if (!mes_load("house/catalog.mes", &catalog)) return;
    MesFileEntry entry;
    for(bool more=mes_find_first(catalog,&entry); more && property_count<100; more=mes_find_next(catalog,&entry)) {
        Property p = {0}; p.slot = entry.num;
        if(sscanf(entry.str,"%d %d %d %d %d %d %63s",&p.price,&p.global,&p.discovered,&p.key,&p.x,&p.y,p.scene)!=7 || p.slot<0 || p.slot>=100 || p.price<=0 || p.global<0 || p.global>=2000 || p.discovered<0 || p.discovered>=2000) continue;
        mes_file_handle_t* handles[]={&p.objects,&p.terrain,&p.roofs,&p.removed};
        const char* files[]={"scene","terrain","roofs","removed"};
        bool valid=true;
        for(int i=0;i<4;i++){char path[160];snprintf(path,sizeof(path),"house/%s/%s.mes",p.scene,files[i]);*handles[i]=MES_FILE_HANDLE_INVALID;if(!mes_load(path,handles[i]))valid=false;}
        p.vegetation=MES_FILE_HANDLE_INVALID;
        if(valid){
            char path[160];snprintf(path,sizeof(path),"house/%s/vegetation.mes",p.scene);
            mes_load(path,&p.vegetation); /* Optional for older scene packs. */
            properties[property_count++]=p;
        }
        else for(int i=0;i<4;i++)if(*handles[i]!=MES_FILE_HANDLE_INVALID)mes_unload(*handles[i]);
    }
    mes_unload(catalog);
    ready = property_count > 0 && mes_load("house/descriptions.mes", &descriptions);
}
const char* modern_house_description(int num)
{
    MesFileEntry e={.num=num};
    return ready && mes_search(descriptions,&e) ? e.str : NULL;
}
static void configure(int64_t obj, Property* p, const char* role, bool created)
{
    int type=obj_field_int32_get(obj,OBJ_F_TYPE);
    if(type==OBJ_TYPE_PORTAL || type==OBJ_TYPE_CONTAINER){
        int field=type==OBJ_TYPE_PORTAL?OBJ_F_PORTAL_FLAGS:OBJ_F_CONTAINER_FLAGS;
        int key=type==OBJ_TYPE_PORTAL?OBJ_F_PORTAL_KEY_ID:OBJ_F_CONTAINER_KEY_ID;
        int flags=obj_field_int32_get(obj,field), updated=flags;
        if(!owned(p)) updated=(flags | 1 | 4 | 16) & ~(8|128);
        else updated=flags & ~(4|16);
        if(updated!=flags)obj_field_int32_set(obj,field,updated);
        if(obj_field_int32_get(obj,key)!=p->key)obj_field_int32_set(obj,key,p->key);
        if(!owned(p))obj_field_int32_set(obj,OBJ_F_FLAGS,obj_field_int32_get(obj,OBJ_F_FLAGS)|OF_INVULNERABLE);
    }
    if(!strcmp(role,"guard"))attach(obj,SAP_DIALOG,owned(p)?0:ESTATE_GUARD);
    if(type==OBJ_TYPE_SCENERY && ((obj_field_int32_get(obj,OBJ_F_CURRENT_AID)>>6)&31)==22)attach(obj,SAP_USE,ESTATE_BED);
    if(type==OBJ_TYPE_NPC && created){
        obj_field_int32_set(obj,OBJ_F_NPC_FACTION,0);
        int flags=obj_field_int32_get(obj,OBJ_F_NPC_FLAGS);
        obj_field_int32_set(obj,OBJ_F_NPC_FLAGS,(flags & ~ONF_KOS)|ONF_KOS_OVERRIDE);
        obj_field_int32_set(obj,OBJ_F_FLAGS,obj_field_int32_get(obj,OBJ_F_FLAGS)|OF_INVULNERABLE);
        if(obj_arrayfield_length_get(obj,OBJ_F_CRITTER_INVENTORY_LIST_IDX)==0)obj_field_int32_set(obj,OBJ_F_CRITTER_INVENTORY_NUM,0);
    }
}
/* Vegetation shades are attached dark lights, separate from sprite visibility.
 * Repair old saves whose OF_OFF was set without disabling those attachments. */
static void hide_scenery(int64_t obj)
{
    if(!(obj_field_int32_get(obj,OBJ_F_FLAGS)&OF_OFF))object_flags_set(obj,OF_OFF);
    bool active=(tig_art_id_t)obj_field_int32_get(obj,OBJ_F_LIGHT_AID)!=TIG_ART_ID_INVALID
        && !(obj_field_int32_get(obj,OBJ_F_LIGHT_FLAGS)&LF_OFF);
    for(int i=0;i<4;i++){
        if(obj_arrayfield_uint32_get(obj,OBJ_F_OVERLAY_LIGHT_AID,i)!=TIG_ART_ID_INVALID
          && !(obj_arrayfield_uint32_get(obj,OBJ_F_OVERLAY_LIGHT_FLAGS,i)&LF_OFF))active=true;
    }
    if(active)light_set_flags(obj,LF_OFF);
    shadow_clear(obj);
}
static void terrain(Property* p, Sector* sec)
{
    MesFileEntry e;
    for(bool more=mes_find_first(p->terrain,&e);more;more=mes_find_next(p->terrain,&e)){
        if(e.num<0 || e.num>=4096)continue;
        uint32_t aid=(uint32_t)strtoul(e.str,NULL,10);
        if(sec->tiles.art_ids[e.num]!=aid){sec->tiles.art_ids[e.num]=aid;sec->tiles.difmask[e.num/32]|=1u<<(e.num%32);sec->tiles.dif=1;}
    }
    for(bool more=mes_find_first(p->roofs,&e);more;more=mes_find_next(p->roofs,&e)){
        if(e.num>=0 && e.num<256){
            uint32_t current=sec->roofs.art_ids[e.num],aid=(uint32_t)strtoul(e.str,NULL,10);
            uint32_t normalized=tig_art_roof_id_fade_set(tig_art_roof_id_fill_set(current,0),0);
            uint32_t expected=tig_art_roof_id_fade_set(tig_art_roof_id_fill_set(aid,0),0);
            if(normalized!=expected){sec->roofs.art_ids[e.num]=aid;sec->roofs.empty=0;wallcheck_recalc(obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION));}
        }
    }
    for(bool more=mes_find_first(p->removed,&e);more;more=mes_find_next(p->removed,&e)){
        long long loc,aid;int dx,dy;
        if(sscanf(e.str,"%lld %lld %d %d",&loc,&aid,&dx,&dy)!=4)continue;
        for(ObjectNode* n=sec->objects.heads[tile_id_from_loc(loc)];n;n=n->next){
            int64_t o=n->obj;int flags=obj_field_int32_get(o,OBJ_F_FLAGS);
            if(!(flags&OF_DYNAMIC) && obj_field_int64_get(o,OBJ_F_LOCATION)==loc
              && (uint32_t)obj_field_int32_get(o,OBJ_F_CURRENT_AID)==(uint32_t)aid
              && obj_field_int32_get(o,OBJ_F_OFFSET_X)==dx && obj_field_int32_get(o,OBJ_F_OFFSET_Y)==dy)
                hide_scenery(o);
        }
    }
}
/* Explicit per-tile scenery masks also cover saved/generated variants. Never
 * remove items, containers, actors, or scenery outside the authored clearance. */
static void clear_vegetation(Property* p, Sector* sec)
{
    if(p->vegetation==MES_FILE_HANDLE_INVALID)return;
    MesFileEntry e;
    for(bool more=mes_find_first(p->vegetation,&e);more;more=mes_find_next(p->vegetation,&e)){
        if(e.num<0 || e.num>=4096)continue;
        unsigned mask=(unsigned)strtoul(e.str,NULL,10) & 0x3Eu;
        int64_t loc=location_make(p->x*64+e.num%64,p->y*64+e.num/64);
        for(ObjectNode* n=sec->objects.heads[tile_id_from_loc(loc)];n;n=n->next){
            int64_t obj=n->obj;
            if(obj_field_int32_get(obj,OBJ_F_TYPE)!=OBJ_TYPE_SCENERY
              || obj_field_int64_get(obj,OBJ_F_LOCATION)!=loc)continue;
            int kind=tig_art_scenery_id_type_get(obj_field_int32_get(obj,OBJ_F_CURRENT_AID));
            if(mask & (1u<<kind))hide_scenery(obj);
        }
    }
}
void modern_house_tick(void)
{
    static unsigned last;
    if(gamelib_in_load() || map_is_clearing_objects() || !on_main_map() || gamelib_ping_time-last<500)return;
    last=gamelib_ping_time;
    for(int i=0;i<property_count;i++){
        Property* p=&properties[i];int64_t sid=SECTOR_MAKE((int64_t)p->x,(int64_t)p->y);
        if(!sector_loaded(sid))continue;
        Sector* sec;if(!sector_lock(sid,&sec))continue;
        terrain(p,sec); sector_unlock(sid);
        MesFileEntry e;
        for(bool more=mes_find_first(p->objects,&e);more;more=mes_find_next(p->objects,&e)){
            if(e.num<100)continue;
            char role[20],id[40];if(sscanf(e.str,"%19s %39s",role,id)!=2)continue;
            int64_t obj=lookup(id);bool created=obj==OBJ_HANDLE_NULL;
            if(created){
                char path[180];snprintf(path,sizeof(path),"house/%s/objects/%s.mob",p->scene,id);
                if(!objf_solitary_read(&obj,path))continue;
                obj_load_postprocess(obj);
            }
            configure(obj,p,role,created);
            if(created)object_install_loaded(obj);
        }
        if(sector_lock(sid,&sec)){clear_vegetation(p,sec);sector_unlock(sid);}
    }
    /* The existing sign identifies Town Hall; place the agent next to it.
     * The immutable GUID prevents duplication on revisits or save migration. */
    int64_t loc=location_make(seller_x,seller_y);
    if(sector_loaded(sector_id_from_loc(loc)) && find_seller()==OBJ_HANDLE_NULL){
        int64_t obj;ObjectID oid=guid(seller_id);
        if(oid.type!=OID_TYPE_GUID)return;
        obj_create_inst_with_oid(sub_4685A0(17078),loc,oid,&obj);
        obj_field_int32_set(obj,OBJ_F_DESCRIPTION,61002);
        obj_field_int32_set(obj,OBJ_F_CRITTER_DESCRIPTION_UNKNOWN,61005);
        obj_field_int32_set(obj,OBJ_F_NAME,61002);
        obj_field_int32_set(obj,OBJ_F_NPC_FACTION,0);
        obj_field_int32_set(obj,OBJ_F_NPC_FLAGS,ONF_KOS_OVERRIDE);
        obj_field_int32_set(obj,OBJ_F_FLAGS,obj_field_int32_get(obj,OBJ_F_FLAGS)|OF_INVULNERABLE|OF_DYNAMIC);
        obj_field_int64_set(obj,OBJ_F_NPC_STANDPOINT_DAY,loc);
        obj_field_int64_set(obj,OBJ_F_NPC_STANDPOINT_NIGHT,loc);
        obj_field_int32_set(obj,OBJ_F_CRITTER_INVENTORY_SOURCE,20);
        obj_field_int32_set(obj,OBJ_F_CRITTER_INVENTORY_NUM,0);
        attach(obj,SAP_DIALOG,ESTATE_AGENT);
        object_install_loaded(obj);
        object_set_current_aid(obj,tig_art_id_rotation_set(obj_field_int32_get(obj,OBJ_F_CURRENT_AID),seller_rotation));
        sub_43E770(obj,loc,seller_dx,seller_dy);
        seller_handle=obj;
    }
}
static bool give_key(Property* p,int64_t pc)
{
    if(sub_463370(pc,p->key))return true;
    int64_t key=OBJ_HANDLE_NULL;
    int64_t proto=sub_4685A0(BP_KEY);
    if(!proto || obj_field_int32_get(proto,OBJ_F_TYPE)!=OBJ_TYPE_KEY)return false;
    if(!object_create(proto,obj_field_int64_get(pc,OBJ_F_LOCATION),&key))return false;
    obj_field_int32_set(key,OBJ_F_KEY_KEY_ID,p->key);
    obj_field_int32_set(key,OBJ_F_DESCRIPTION,p->key);
    obj_field_int32_set(key,OBJ_F_NAME,p->key);
    if(!item_transfer(key,pc)){object_destroy(key);return false;}
    return true;
}
bool modern_house_script(ScriptInvocation* inv,bool* result)
{
    if(!ready || !inv || !inv->script)return false;
    int script=inv->script->num;
    if(script!=ESTATE_AGENT && script!=ESTATE_GUARD && script!=ESTATE_BED)return false;
    *result=false;
    if(!on_main_map())return true;
    if(script==ESTATE_BED){
        int64_t sector=sector_id_from_loc(obj_field_int64_get(inv->attachee_obj,OBJ_F_LOCATION));
        for(int i=0;i<property_count;i++)if(sector==SECTOR_MAKE((int64_t)properties[i].x,(int64_t)properties[i].y))*result=!owned(&properties[i]);
        return true;
    }
    if(!player_is_local_pc_obj(inv->triggerer_obj) || inv->attachment_point!=SAP_DIALOG)return true;
    if(script==ESTATE_AGENT && inv->attachee_obj!=find_seller())return true;
    if(inv->line==1)return true; /* Ordinary dialogue exit resumes after row zero. */
    int line=1;
    if(script==ESTATE_GUARD){
        int64_t sid=sector_id_from_loc(obj_field_int64_get(inv->attachee_obj,OBJ_F_LOCATION));
        for(int i=0;i<property_count;i++)if(sid==SECTOR_MAKE((int64_t)properties[i].x,(int64_t)properties[i].y)){script_global_var_set(properties[i].discovered,1);line=1+properties[i].slot*10;}
    }
    if(script==ESTATE_AGENT && inv->line>=100){
        int slot=(inv->line-100)/10,command=(inv->line-100)%10;
        Property* p=NULL;for(int i=0;i<property_count;i++)if(properties[i].slot==slot)p=&properties[i];
        if(!p || command>2)return true;
        script_global_var_set(p->discovered,1);
        int base=3000+slot*100;line=base;
        if(owned(p)){
            line=base+20;
            if(command==2){
                if(sub_463370(inv->triggerer_obj,p->key))line=base+60;
                else line=base+(give_key(p,inv->triggerer_obj)?70:50);
            }
        }else if(command==1){
            int64_t pc=inv->triggerer_obj;
            if(item_gold_get(pc)<p->price)line=base+40;
            else if(!item_gold_transfer(pc,OBJ_HANDLE_NULL,p->price,OBJ_HANDLE_NULL))line=base+50;
            else if(!give_key(p,pc)){
                item_gold_transfer(OBJ_HANDLE_NULL,pc,p->price,OBJ_HANDLE_NULL);line=base+50;
            }else{
                script_global_var_set(p->global,1);script_global_var_set(p->discovered,1);line=base+30;
                /* Reconfigure loaded locks and guard now, without regenerating items. */
                MesFileEntry e;for(bool more=mes_find_first(p->objects,&e);more;more=mes_find_next(p->objects,&e)){
                    char role[20],id[40];if(e.num>=100 && sscanf(e.str,"%19s %39s",role,id)==2){int64_t o=lookup(id);if(o)configure(o,p,role,false);}
                }
            }
        }
    }
    dialog_ui_start_dialog(inv->triggerer_obj,inv->attachee_obj,script,0,line);
    return true;
}

bool modern_house_location(int index, int* x, int* y, const char** name)
{
    if(!ready || index<0 || index>=100)return false;
    Property* p=NULL;
    for(int i=0;i<property_count;i++)if(properties[i].slot==index)p=&properties[i];
    if(!p)return false;
    if(!owned(p) && !script_global_var_get(p->discovered))return false;
    *x=p->x;*y=p->y;*name=modern_house_description(70000+p->slot);
    return *name!=NULL;
}
