#include "worlded/npc_routes.hpp"
#include "worlded/building.hpp"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>

namespace worlded {
namespace {
void put(Bytes& bytes, std::uint32_t value) { for (unsigned n=0;n<4;++n) bytes.push_back((value>>(8*n))&255); }
Bytes bytes(const std::string& s) { return Bytes(s.begin(),s.end()); }
std::string new_id() {
    std::random_device random;
    std::ostringstream out; out<<"0200"<<std::hex<<std::setfill('0');
    for (unsigned n=0;n<16;++n) {
        unsigned b=random()&255;
        if (n==7) b=(b&15)|64; // GUID layout stores its version word little-endian.
        if (n==8) b=(b&63)|128;
        out<<std::setw(2)<<b;
    }
    return out.str();
}
std::uint32_t wall_aid(unsigned structure,unsigned piece,unsigned direction) {
    bool flip=direction/2==1 || direction/2==3;
    return 0x10000000u | (structure<<20) | (piece<<14) | (direction<<11) | (flip ? 17 : 0);
}
Sector blank_sector(unsigned tile) {
    Bytes data; put(data,0); for (unsigned n=0;n<4096;++n) put(data,tile);
    put(data,1); put(data,0xaa0004); put(data,0);
    data.insert(data.end(),12+24+512,0); put(data,0); return Sector(std::move(data));
}
}
GameObject clone_instance(const GameObject& object) { auto result=object; result.id=new_id(); result.editor_id=result.id; return result; }
Sector Building::sector(std::size_t count) const {
    auto data=ground.encode(); data.resize(ground.info().object_offset);
    count=std::min(count,objects.size());
    for (std::size_t n=0;n<count;++n) { auto object=encode_instance(objects[n]); data.insert(data.end(),object.begin(),object.end()); }
    put(data,unsigned(count)); return Sector(std::move(data));
}
Building build_house(const Installation& installation, const BuildingPlan& plan) {
    if (plan.width<8 || plan.width>24 || plan.depth<8 || plan.depth>24
        || plan.x<2 || plan.y<2 || plan.x+plan.width>61 || plan.y+plan.depth>61
        || plan.chests<1 || plan.chests>8 || plan.chests>plan.width-3 || plan.structure>255 || plan.door_art>511)
        throw std::runtime_error("Building dimensions, position or resource IDs out of range");
    Building building{plan,blank_sector(plan.ground),{}, {}};
    std::map<unsigned,GameObject> prototypes;
    for (auto [id,type] : {std::pair{plan.wall_prototype,0u},{plan.door_prototype,1u},{plan.chest_prototype,2u}}) {
        bool found=false;
        for (const auto& [name,sources] : installation.resources()) {
            (void)sources;
            if (name.rfind("proto/",0)!=0 || name.substr(name.size()-4)!=".pro") continue;
            unsigned number=0; auto first=name.data()+6;
            if (std::from_chars(first,name.data()+name.size(),number).ec!=std::errc{} || number!=id) continue;
            auto data=installation.read(name); auto objects=read_objects(data);
            if (objects.size()!=1 || !objects[0].prototype || objects[0].type!=type || objects[0].prototype_id!=id)
                throw std::runtime_error("Building prototype has unexpected type or ID");
            prototypes[id]=objects[0]; building.prototypes.emplace(name,std::move(data)); found=true; break;
        }
        if (!found) throw std::runtime_error("Missing building prototype: "+std::to_string(id));
    }
    auto add=[&](unsigned proto,unsigned x,unsigned y,unsigned aid) -> GameObject& {
        const auto& base=prototypes.at(proto); GameObject object;
        object.type=base.type; object.prototype_id=proto; object.id=new_id();
        set_number(object,"LOCATION",std::uint64_t(x)|(std::uint64_t(y)<<32));
        set_number(object,"CURRENT_AID",aid); set_number(object,"AID",aid);
        set_number(object,"OFFSET_X",0); set_number(object,"OFFSET_Y",0);
        // Purpose-built static objects: no inherited quest hooks or transient flags.
        // Preserve unrelated prototype flags, but clear destroyed/off/invisible,
        // flat/text, dynamic, inventory and no-block states (obj_flags.h).
        auto flags=std::uint32_t(base.number("FLAGS"));
        flags &= ~(0x1u|0x2u|0x4u|0x8u|0x100u|0x200u|0x400u|0x800u|0x1000u|0x2000u|0x800000u);
        set_number(object,"FLAGS",flags); set_null(object,"SCRIPTS_IDX");
        building.objects.push_back(std::move(object)); return building.objects.back();
    };
    unsigned left=plan.x, top=plan.y, right=left+plan.width, bottom=top+plan.depth;
    for (unsigned y=top;y<=bottom;++y) for (unsigned x=left;x<=right;++x) building.ground.set_tile(x,y,plan.floor);
    // Back faces use interior sprites; front faces use exterior sprites.
    unsigned doorway=left+plan.width/2;
    for (unsigned x=left;x<right;++x) add(plan.wall_prototype,x,top,wall_aid(plan.structure,0,6));
    for (unsigned y=top;y<bottom;++y) add(plan.wall_prototype,left,y,wall_aid(plan.structure,0,0));
    for (unsigned y=top;y<bottom;++y) add(plan.wall_prototype,right,y,wall_aid(plan.structure,0,4));
    for (unsigned x=left;x<right;++x) {
        unsigned piece=0;
        if (x==doorway-1) piece=21;
        if (x==doorway) piece=22;
        if (x==doorway+1) piece=23;
        auto& wall=add(plan.wall_prototype,x,bottom,wall_aid(plan.structure,piece,2));
        if (piece==22) set_number(wall,"FLAGS",wall.number("FLAGS")|0x400u); // Opening frame does not block passage.
    }
    auto& door=add(plan.door_prototype,doorway,bottom,0x30000400u|(plan.door_art<<19)|(2u<<11)|17u);
    set_number(door,"PORTAL_FLAGS",8); // Unlocked test door; purchase gating is a separate milestone.
    for (unsigned i=0;i<plan.chests;++i) {
        auto& chest=add(plan.chest_prototype,left+2+i,top+2,0x70000040u|(1u<<19)|(4u<<11));
        set_number(chest,"CONTAINER_FLAGS",8);
        set_number(chest,"CONTAINER_INVENTORY_NUM",0); set_number(chest,"CONTAINER_INVENTORY_SOURCE",0);
        set_null(chest,"CONTAINER_INVENTORY_LIST_IDX");
    }
    return building;
}
Building insert_room(const Installation& installation,const Building& scene,const BuildingPlan& plan,bool clear_ground) {
    auto room=build_house(installation,plan); auto result=scene; result.plan=plan;
    auto inFootprint=[&](const GameObject& object) {auto loc=object.number("LOCATION"); unsigned x=loc&63,y=(loc>>32)&63;
        return x>=plan.x-2 && x<=plan.x+plan.width+2 && y>=plan.y-2 && y<=plan.y+plan.depth+4;};
    if(clear_ground) result.objects.erase(std::remove_if(result.objects.begin(),result.objects.end(),inFootprint),result.objects.end());
    for(unsigned y=plan.y-2;y<=plan.y+plan.depth+4 && y<64;++y) for(unsigned x=plan.x-2;x<=plan.x+plan.width+2 && x<64;++x) {
        bool inside=x>=plan.x && x<=plan.x+plan.width && y>=plan.y && y<=plan.y+plan.depth;
        if(inside || clear_ground) result.ground.set_tile(x,y,inside ? plan.floor : plan.ground);
    }
    result.objects.insert(result.objects.end(),room.objects.begin(),room.objects.end());
    result.prototypes.insert(room.prototypes.begin(),room.prototypes.end());
    return result;
}
std::filesystem::path export_building(const Building& building,const std::filesystem::path& project) {
    // Complete all encoding before creating the destination. Existing projects are
    // refused, including previous exports: each export is an immutable snapshot.
    Building statics=building;
    // CE object_is_static: containers, projectiles, items and critters are MOBs;
    // walls, portals, scenery and traps are static unless OF_DYNAMIC is set.
    auto mobile=[](const GameObject& object) { return object.type==2 || (object.type>=4 && object.type<=16) || (object.number("FLAGS")&0x2000); };
    statics.objects.erase(std::remove_if(statics.objects.begin(),statics.objects.end(),mobile),statics.objects.end());
    // SEC and DIF must enumerate objects identically. CE saves differences in
    // tile/render order, not creation order (sector_object_list.c).
    std::array<std::vector<GameObject>,4096> cells;
    auto depth=[](const GameObject& o) {
        int x=std::int32_t(o.number("OFFSET_X")),y=std::int32_t(o.number("OFFSET_Y"));
        if(o.type<=1){unsigned rotation=(o.number("CURRENT_AID")>>11)&7;x=0;y=rotation>1 && rotation<6?19:-20;}
        int a=(x-40)/2,b=2*(y/2);return std::pair{a+b,b-a};
    };
    for(const auto& o:statics.objects){auto loc=o.number("LOCATION");auto& cell=cells[(loc&63)|(((loc>>32)&63)<<6)];auto at=cell.begin();
        for(;at!=cell.end();++at){auto flags=at->number("FLAGS");
            if(o.number("FLAGS")&4){if(!(flags&4) || (o.type==3 && (o.number("SCENERY_FLAGS")&0x200)))break;}
            else if(o.type==0 && at->type==1)break;
            else if(o.type==1 && at->type==0){++at;break;}
            if(!(flags&4) && depth(o)<depth(*at))break;
        }
        cell.insert(at,o);
    }
    statics.objects.clear();for(auto& cell:cells)statics.objects.insert(statics.objects.end(),cell.begin(),cell.end());
    auto originX=(building.sector_id&0x3ffffff)*64, originY=(building.sector_id>>26)*64;
    auto globalize=[&](GameObject object) { auto loc=object.number("LOCATION");
        set_number(object,"LOCATION",(originX+std::uint32_t(loc))|((originY+(loc>>32))<<32));
        transform_npc_routes(object,building.sector_id,true);
        return object; };
    for (auto& object : statics.objects) object=globalize(object);
    auto sector=statics.sector().encode();
    std::set<std::string> ids;
    for (const auto& object : building.objects)
        if (object.id.substr(0,4)=="0200" && !ids.insert(object.id).second) throw std::runtime_error("Duplicate object ID in building");
    Bytes properties; put(properties,0); put(properties,0); put(properties,64); put(properties,0); put(properties,64); put(properties,0);
    // TerrainHeader v1.2, uncompressed one-sector terrain; no jump points yet.
    Bytes terrain; put(terrain,0x3f99999a); put(terrain,0); put(terrain,1); put(terrain,0);
    put(terrain,1); put(terrain,0); put(terrain,0); put(terrain,0);
    terrain.push_back(15u<<2); terrain.push_back(0);
    const auto& p=building.plan;
    auto map=resource_name(building.map_name);
    if (map.find('/')!=std::string::npos || map.find('"')!=std::string::npos) throw std::runtime_error("Invalid map name");
    auto folder="exports/maps/"+map+"/";
    std::ostringstream manifest;
    manifest<<"{\"format\":\"worlded-scene\",\"version\":1,\"map\":\""<<map<<"\",\"sector\":"<<building.sector_id
        <<",\"x\":"<<p.x<<",\"y\":"<<p.y<<",\"width\":"<<p.width<<",\"depth\":"<<p.depth<<",\"objects\":"<<building.objects.size()
        <<",\"existing_map\":"<<(building.existing_map?"true":"false")<<",\"campaign_installed\":false}\n";
    std::map<std::string,Bytes> files;
    files[folder+std::to_string(building.sector_id)+".sec"]=sector;
    for (const auto& object : building.objects) if (mobile(object))
        files[folder+"g_"+object.id.substr(4)+".mob"]=encode_instance(globalize(object));
    if (!building.existing_map) {
        files[folder+"map.prp"]=properties; files[folder+"terrain.tdf"]=terrain; files[folder+"map.jmp"]=Bytes(4,0);
        files[folder+"startloc.txt"]=bytes(std::to_string(p.x+p.width/2)+"\n"+std::to_string(p.y+p.depth+2)+"\n");
        for (const auto& [path,data] : building.prototypes) files["exports/"+path]=data;
    }
    std::ostringstream translations; translations<<"{";
    bool first=true;
    auto quote=[](const std::string& value) {std::string result="\""; for(unsigned char c:value) {
        if(c=='"' || c=='\\') result+='\\';
        if(c<32) throw std::runtime_error("Location labels cannot contain control characters");
        result+=char(c);
    } return result+'"';};
    for(const auto& [lang,label]:building.location_names) {
        if(lang.empty() || lang.find_first_not_of("abcdefghijklmnopqrstuvwxyz-")!=std::string::npos
            || label.empty() || label.find_first_of("{}")!=std::string::npos) throw std::runtime_error("Invalid language or location label");
        if(!first) translations<<',';
        first=false; translations<<quote(lang)<<":"<<quote(label);
    }
    translations<<"}\n";
    for(const auto& [path,data]:building.content){
        auto slash=path.find('/');auto group=path.substr(0,slash),name=slash==std::string::npos?"":path.substr(slash+1);
        if((group!="scr" && group!="dlg" && group!="script-sources") || name.empty() || name=="." || name==".." || name.find_first_of("/\\")!=std::string::npos || resource_name(path)!=path)throw std::runtime_error("Invalid project content path");
        files[(group=="script-sources"?"":"exports/")+path]=data;
    }
    files["location-names.json"]=bytes(translations.str());
    files["building.json"]=bytes(manifest.str());
    files["README.txt"]=bytes("WorldED scene export for isolated testing. Original installation unchanged. Existing-map exports contain sector overrides only; campaign/save compatibility must be validated separately. Resources from your installation are for local use.\n");
    create_project(project);
    for (const auto& [name,data] : files) export_resource(project,name,data);
    auto result=project/std::filesystem::u8path(cp1252_utf8(folder+std::to_string(building.sector_id)+".sec"));
    return result;
}
}
