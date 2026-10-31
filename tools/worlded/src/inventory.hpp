#pragma once
#include "worlded/building.hpp"
#include <algorithm>
#include <functional>
#include <set>

namespace worlded {
inline bool inventory_owner(const GameObject& object) { return object.type==2 || object.type==15 || object.type==16; }
inline bool inventory_item(const GameObject& object) { return object.type>=5 && object.type<=14; }
inline const char* inventory_list_field(const GameObject& owner) { return owner.type==2?"CONTAINER_INVENTORY_LIST_IDX":"CRITTER_INVENTORY_LIST_IDX"; }
inline std::string inventory_parent(const GameObject& object) {
    auto field=object.field("ITEM_PARENT");return field && field->present?field->text:std::string{};
}
inline Bytes inventory_id_bytes(const std::string& id) {
    if(id.size()!=36)throw std::runtime_error("Invalid persistent inventory identifier");
    Bytes bytes;auto digit=[](char c)->unsigned{if(c>='0' && c<='9')return c-'0';if(c>='a' && c<='f')return c-'a'+10;throw std::runtime_error("Invalid inventory identifier hex");};
    for(unsigned i=0;i<18;i++){if(i==2)bytes.insert(bytes.end(),6,0);bytes.push_back((digit(id[i*2])<<4)|digit(id[i*2+1]));}return bytes;
}
inline std::string inventory_bytes_id(const Bytes& bytes) {
    if(bytes.size()!=24)throw std::runtime_error("Invalid inventory reference size");
    static const char* hex="0123456789abcdef";std::string result;
    for(unsigned i=0;i<24;i++){if(i>=2 && i<8)continue;result+=hex[bytes[i]>>4];result+=hex[bytes[i]&15];}return result;
}
inline void inventory_set_field(GameObject& object,ObjectField field) {
    for(auto& previous:object.fields)if(previous.name==field.name){previous=std::move(field);return;}object.fields.push_back(std::move(field));
}
inline void inventory_set_handle(GameObject& object,const char* name,const std::string& id) {
    ObjectField field;field.name=name;field.kind="HANDLE";field.present=!id.empty();field.text=id;inventory_set_field(object,std::move(field));
}
inline std::vector<std::string> inventory_ids(const GameObject& owner) {
    if(!inventory_owner(owner))throw std::runtime_error("Select a container or character to edit its inventory");
    std::vector<std::string> result;auto field=owner.field(inventory_list_field(owner));
    if(field && field->present){auto elements=field->elements;std::sort(elements.begin(),elements.end(),[](const auto& a,const auto& b){return a.index<b.index;});
        for(const auto& entry:elements){if(entry.index!=result.size())throw std::runtime_error("Inventory contains a missing list entry");result.push_back(inventory_bytes_id(entry.data));}}
    auto count=owner.number(owner.type==2?"CONTAINER_INVENTORY_NUM":"CRITTER_INVENTORY_NUM");
    if(count!=result.size())throw std::runtime_error("Inventory count does not match its references");return result;
}
inline std::vector<GameObject> inventory_contents(const std::vector<GameObject>& objects,const GameObject& owner) {
    std::vector<GameObject> result;std::set<std::string> seen;
    for(const auto& id:inventory_ids(owner)){
        if(!seen.insert(id).second)throw std::runtime_error("Duplicate item in inventory");
        auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==id;});
        if(found==objects.end())throw std::runtime_error("Inventory item is outside this project: "+id+". Open a project containing its MOB before editing this inventory.");
        if(!inventory_item(*found) || !(found->number("FLAGS")&0x1000) || inventory_parent(*found)!=owner.id)throw std::runtime_error("Inventory item has an inconsistent owner: "+id);
        result.push_back(*found);
    }return result;
}
inline int inventory_equipment_slot(const GameObject& resolved) {
    if(resolved.type==5)return 1004;
    if(resolved.type==14 && (resolved.number("GENERIC_FLAGS")&1))return 1005;
    if(resolved.type==7){static const int equipmentSlots[]={1006,1005,1000,1007,1008,1001,1003};auto coverage=(resolved.number("ITEM_INV_AID")>>14)&7;if(coverage<7)return equipmentSlots[coverage];}
    return -1;
}
using InventorySize=std::function<std::pair<unsigned,unsigned>(const GameObject&)>;
using InventoryResolve=std::function<GameObject(const GameObject&)>;
// Match the native 10-column grid and ITEM_INV_LOC_* slots. Runtime race,
// gender, art availability and spell restrictions remain engine decisions.
inline void inventory_validate_slots(const GameObject& owner,const std::vector<GameObject>& items,const InventoryResolve& resolve,const InventorySize& size) {
    std::set<unsigned> cells,equipment;bool fixedTwoHands=false,shield=false;
    for(const auto& item:items){if(!inventory_item(item))throw std::runtime_error("Only items can be placed in an inventory");
        auto resolved=resolve(item);auto slot=item.number("ITEM_INV_LOCATION");
        if(slot>=1000 && slot<=1008){
            if(owner.type==2)throw std::runtime_error("Containers cannot equip items");
            int expected=inventory_equipment_slot(resolved);if(expected!=int(slot) && !(expected==1001 && slot==1002))throw std::runtime_error("Item does not fit the selected equipment slot");
            if(!equipment.insert(unsigned(slot)).second)throw std::runtime_error("Two items use the same equipment slot");
            if(slot==1005)shield=true;if(slot==1004 && (resolved.number("WEAPON_FLAGS")&0x0c)==0x0c)fixedTwoHands=true;
        }else{
            auto dimensions=size(resolved);unsigned width=dimensions.first,height=dimensions.second,rows=owner.type==2?96:12;
            if(!width || !height || slot>=rows*10 || slot%10+width>10 || slot/10+height>rows)throw std::runtime_error("Item extends beyond the inventory grid");
            for(unsigned y=0;y<height;y++)for(unsigned x=0;x<width;x++)if(!cells.insert(unsigned(slot)+y*10+x).second)throw std::runtime_error("Inventory items overlap");
        }
    }if(fixedTwoHands && shield)throw std::runtime_error("A fixed two-handed weapon cannot be equipped with a shield");
}
inline std::vector<GameObject> inventory_apply(const std::vector<GameObject>& objects,const std::string& ownerId,std::vector<GameObject> contents,const InventoryResolve& resolve,const InventorySize& size) {
    auto found=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==ownerId;});
    if(found==objects.end())throw std::runtime_error("Inventory owner is missing");auto owner=*found;
    auto previous=inventory_contents(objects,owner);inventory_validate_slots(owner,contents,resolve,size);
    std::set<std::string> oldIds,newIds;for(const auto& item:previous)oldIds.insert(item.id);
    ObjectField list;list.name=inventory_list_field(owner);list.kind="HANDLE_ARRAY";
    if(owner.type!=2)for(auto field:{"CRITTER_GOLD","CRITTER_ARROWS","CRITTER_BULLETS","CRITTER_POWER_CELLS","CRITTER_FUEL"})inventory_set_handle(owner,field,{});
    for(auto& item:contents){
        if(item.id.substr(0,4)!="0200" || !newIds.insert(item.id).second)throw std::runtime_error("Inventory items need unique persistent GUIDs");
        if(!oldIds.count(item.id) && std::any_of(objects.begin(),objects.end(),[&](const auto& o){return o.id==item.id;}))throw std::runtime_error("Item already belongs to this project; duplicate it before adding it");
        auto resolved=resolve(item);set_number(item,"FLAGS",resolved.number("FLAGS")|0x1000u);set_number(item,"LOCATION",owner.number("LOCATION"));inventory_set_handle(item,"ITEM_PARENT",owner.id);
        list.elements.push_back({unsigned(list.elements.size()),inventory_id_bytes(item.id)});
        if(owner.type!=2){const char* stack=nullptr;if(item.type==8)stack="CRITTER_GOLD";else if(item.type==6){static const char* fields[]={"CRITTER_ARROWS","CRITTER_BULLETS","CRITTER_POWER_CELLS","CRITTER_FUEL"};auto ammo=resolved.number("AMMO_TYPE");if(ammo<4)stack=fields[ammo];}
            if(stack){auto existing=owner.field(stack);if(existing && existing->present)throw std::runtime_error("Use one gold or ammunition stack per ammunition type");inventory_set_handle(owner,stack,item.id);}}
        encode_instance(item);
    }
    inventory_set_field(owner,std::move(list));set_number(owner,owner.type==2?"CONTAINER_INVENTORY_NUM":"CRITTER_INVENTORY_NUM",contents.size());
    set_number(owner,owner.type==2?"CONTAINER_INVENTORY_SOURCE":"CRITTER_INVENTORY_SOURCE",0);
    if(owner.type==16)set_number(owner,"NPC_FLAGS",resolve(owner).number("NPC_FLAGS")&~0x40u);
    std::vector<GameObject> result;for(const auto& object:objects){if(object.id==owner.id)result.push_back(owner);else if(!oldIds.count(object.id))result.push_back(object);}
    result.insert(result.end(),contents.begin(),contents.end());return result;
}
// Transfer existing ground instances without cloning GUIDs or changing unrelated contents.
inline std::vector<GameObject> inventory_transfer(const std::vector<GameObject>& objects,const std::string& ownerId,const std::set<std::string>& ids,const InventoryResolve& resolve,const InventorySize& size) {
    auto owner=std::find_if(objects.begin(),objects.end(),[&](const auto& o){return o.id==ownerId;});
    if(owner==objects.end() || !inventory_owner(*owner) || ownerId.substr(0,4)!="0200")throw std::runtime_error("Choose a project container or character with a persistent identity.");
    if(ids.empty() || ids.count(ownerId))throw std::runtime_error("Select ground items to transfer.");
    auto contents=inventory_contents(objects,*owner);
    inventory_validate_slots(*owner,contents,resolve,size);
    std::set<std::string> found;
    for(const auto& object:objects)if(ids.count(object.id)){
        if(!found.insert(object.id).second || !inventory_item(object) || object.id.substr(0,4)!="0200" || (resolve(object).number("FLAGS")&0x1000u))throw std::runtime_error("Only ground items with unique identities can be transferred.");
        auto item=object;bool placed=false;unsigned capacity=owner->type==2?960:120;
        for(unsigned slot=0;slot<capacity;slot++){
            set_number(item,"ITEM_INV_LOCATION",slot);auto trial=contents;trial.push_back(item);
            try{inventory_validate_slots(*owner,trial,resolve,size);placed=true;break;}catch(const std::exception&){}
        }
        if(!placed)throw std::runtime_error("No room in this inventory. The items have not been moved.");
        contents.push_back(std::move(item));
    }
    if(found!=ids)throw std::runtime_error("A selected item is missing from the project.");
    auto remaining=objects;remaining.erase(std::remove_if(remaining.begin(),remaining.end(),[&](const auto& o){return ids.count(o.id);}),remaining.end());
    return inventory_apply(remaining,ownerId,std::move(contents),resolve,size);
}
// Expand before copy/delete, then remap copied identities in one transaction.
inline std::set<std::string> inventory_descendants(const std::vector<GameObject>& objects,std::set<std::string> ids) {
    bool changed;do{changed=false;for(const auto& object:objects)if(inventory_item(object) && ids.count(inventory_parent(object)))changed|=ids.insert(object.id).second;}while(changed);return ids;
}
inline void inventory_remap(GameObject& object,const std::map<std::string,std::string>& ids) {
    for(auto& field:object.fields){if(!field.present)continue;
        if(field.kind=="HANDLE"){auto found=ids.find(field.text);if(found!=ids.end())field.text=found->second;}
        else if(field.kind=="HANDLE_ARRAY")for(auto& element:field.elements){auto found=ids.find(inventory_bytes_id(element.data));if(found!=ids.end())element.data=inventory_id_bytes(found->second);}
    }
}
}
