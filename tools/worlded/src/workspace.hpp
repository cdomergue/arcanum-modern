#pragma once
#include "playtest.hpp"
#include <QCryptographicHash>

namespace worlded {
inline QByteArray scene_fingerprint(const Building& scene) {
    auto bytes=scene.sector().encode();QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(QByteArrayView(reinterpret_cast<const char*>(bytes.data()),bytes.size()));
    QJsonObject names;for(const auto& [lang,name]:scene.location_names)names[QString::fromStdString(lang)]=QString::fromStdString(name);
    hash.addData(QJsonDocument(names).toJson(QJsonDocument::Compact));
    if(scene.reference){hash.addData(QByteArray::fromStdString(scene.reference->id));hash.addData(QByteArray::number(scene.reference->origins.size()));}
    hash.addData(QByteArrayView(reinterpret_cast<const char*>(scene.update_history.data()),scene.update_history.size()));
    for(const auto& [path,data]:scene.content){hash.addData(QByteArray::fromStdString(path));hash.addData(QByteArray::number(data.size()));hash.addData(QByteArrayView(reinterpret_cast<const char*>(data.data()),data.size()));}return hash.result();
}
struct SceneChanges {int added=0,removed=0,changed=0,tiles=0,roofs=0,blocks=0;bool names=false,scripts=false,content=false,settings=false,lights=false,updates=false;};
inline SceneChanges scene_changes(const Building& scene,const Building& saved) {
    auto key=[](const GameObject& o){return o.editor_id.empty()?o.id:o.editor_id;};
    std::map<std::string,Bytes> before;for(const auto& o:saved.objects)before[key(o)]=encode_instance(o);
    SceneChanges result;
    for(const auto& o:scene.objects){auto it=before.find(key(o));if(it==before.end())result.added++;else {if(it->second!=encode_instance(o))result.changed++;before.erase(it);}}
    result.removed=before.size();
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)if(scene.ground.tile(x,y)!=saved.ground.tile(x,y))result.tiles++;
    for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++)if(scene.ground.roof(x,y)!=saved.ground.roof(x,y))result.roofs++;
    for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)if(scene.ground.blocked(x,y)!=saved.ground.blocked(x,y))result.blocks++;
    auto scripts=[](const Sector& s){std::vector<std::array<std::uint32_t,4>> result;for(const auto& x:s.scripts())result.push_back({x.tile,x.flags,x.counters,x.number});return result;};
    result.lights=scene.ground.lights()!=saved.ground.lights();
    result.scripts=scripts(scene.ground)!=scripts(saved.ground);
    auto a=scene.ground.settings(),b=saved.ground.settings();result.settings=a.aptitude!=b.aptitude || a.light!=b.light || a.music!=b.music || a.ambient!=b.ambient;
    result.updates=scene.update_history!=saved.update_history || bool(scene.reference)!=bool(saved.reference) || (scene.reference && saved.reference && (scene.reference->id!=saved.reference->id || scene.reference->origins.size()!=saved.reference->origins.size()));
    result.content=scene.content!=saved.content;result.names=scene.location_names!=saved.location_names;return result;
}
// Save an owned scene project with rollback. Construct the entire next version
// before moving the old directory; retain user files outside the scene exports.
inline void save_scene_project(const Building& scene,const std::filesystem::path& path) {
    namespace fs=std::filesystem;
    if(!fs::exists(path)){export_building(scene,path);return;}
    if(fs::is_symlink(path) || !fs::is_regular_file(path/".worlded-project"))throw std::runtime_error("This folder is not a WorldED project.");
    auto original=project_json(path/"building.json");
    if(original["format"]!="worlded-scene" || original["map"].toString().toStdString()!=scene.map_name
        || std::uint64_t(original["sector"].toDouble())!=scene.sector_id)throw std::runtime_error("This folder contains another zone. Use Save as.");
    for(const auto& file:fs::recursive_directory_iterator(path))if(file.is_symlink())throw std::runtime_error("The project contains a symbolic link.");
    auto suffix=QUuid::createUuid().toString(QUuid::Id128).toStdString();
    auto stage=path.parent_path()/(path.filename().string()+".saving-"+suffix);
    auto backup=path.parent_path()/(path.filename().string()+".previous-"+suffix);
    try {
        export_building(scene,stage);
        for(const auto& file:fs::recursive_directory_iterator(path)) if(file.is_regular_file()) {
            auto rel=file.path().lexically_relative(path);auto name=rel.generic_string();
            if(name.rfind("exports/",0)==0 || name.rfind("script-sources/",0)==0 || fs::exists(stage/rel))continue;
            fs::create_directories((stage/rel).parent_path());fs::copy_file(file.path(),stage/rel);
        }
        fs::rename(path,backup);
        try{fs::rename(stage,path);}catch(...){fs::rename(backup,path);throw;}
        std::error_code ec;fs::remove_all(backup,ec);
    }catch(...){std::error_code ec;fs::remove_all(stage,ec);throw;}
}
struct SceneFragment {
    std::vector<GameObject> objects;
    std::map<std::pair<int,int>,std::uint32_t> tiles;
    std::map<std::pair<int,int>,std::uint32_t> roofs; // offsets in ground tiles, multiples of four
    std::map<std::string,Bytes> prototypes;
    int width=0,height=0;
    QString source,action;
    bool empty() const {return objects.empty() && tiles.empty() && roofs.empty();}
};
inline SceneFragment copy_fragment(const Installation& installation,const Building& scene,const std::set<std::string>& keys,bool withFloor) {
    SceneFragment result;int minX=64,minY=64,maxX=0,maxY=0;
    for(const auto& object:scene.objects)if(keys.count(object.editor_id.empty()?object.id:object.editor_id)) {
        if(object.type>3)throw std::runtime_error("Building copies support walls, doors, chests and scenery. Remove other objects from the selection.");
        auto loc=object.number("LOCATION");int x=loc&63,y=(loc>>32)&63;minX=std::min(minX,x);minY=std::min(minY,y);maxX=std::max(maxX,x);maxY=std::max(maxY,y);
        // New construction instances carry appearance, not quest links, inventory
        // handles or state belonging to the original campaign object.
        GameObject copy;copy.type=object.type;copy.prototype_id=object.prototype_id;
        for(const char* field:{"LOCATION","AID","CURRENT_AID","OFFSET_X","OFFSET_Y","LIGHT_AID","LIGHT_COLOR","LIGHT_RADIUS"})
            if(object.field(field))set_number(copy,field,object.number(field));
        set_null(copy,"SCRIPTS_IDX");
        if(object.field("FLAGS"))set_number(copy,"FLAGS",object.number("FLAGS")&~(0x1u|0x2u|0x4u|0x8u|0x100u|0x200u|0x800u|0x1000u|0x2000u|0x800000u));
        if(copy.type==1){set_number(copy,"PORTAL_FLAGS",8);set_number(copy,"PORTAL_KEY_ID",0);set_number(copy,"PORTAL_NOTIFY_NPC",0);set_number(copy,"PORTAL_LOCK_DIFFICULTY",0);}
        if(copy.type==2){set_number(copy,"CONTAINER_FLAGS",8);set_number(copy,"CONTAINER_KEY_ID",0);set_number(copy,"CONTAINER_NOTIFY_NPC",0);set_number(copy,"CONTAINER_LOCK_DIFFICULTY",0);set_number(copy,"CONTAINER_INVENTORY_NUM",0);set_number(copy,"CONTAINER_INVENTORY_SOURCE",0);set_null(copy,"CONTAINER_INVENTORY_LIST_IDX");}
        result.objects.push_back(copy);
        if(scene.prototypes.size())result.prototypes.insert(scene.prototypes.begin(),scene.prototypes.end());
        for(const auto& [name,sources]:installation.resources())if(name.rfind("proto/",0)==0 && QString::fromStdString(name).mid(6).section('.',0,0).toUInt()==copy.prototype_id){result.prototypes[name]=installation.read(name);break;}
    }
    if(result.objects.empty())return result;
    result.width=maxX-minX+1;result.height=maxY-minY+1;
    for(auto& object:result.objects){auto loc=object.number("LOCATION");set_number(object,"LOCATION",std::uint64_t((loc&63)-minX)|(std::uint64_t(((loc>>32)&63)-minY)<<32));}
    if(withFloor)for(int y=minY;y<=maxY;y++)for(int x=minX;x<=maxX;x++)result.tiles[{x-minX,y-minY}]=scene.ground.tile(x,y);
    return result;
}
}
