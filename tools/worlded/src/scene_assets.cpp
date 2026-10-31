#include "scene_assets.hpp"
#include "worlded/building.hpp"
#include <QImage>
#include <sstream>
#include <algorithm>
#include <charconv>
#include <set>

using namespace worlded;
namespace {
QString display(const std::string& s) { return QString::fromUtf8(cp1252_utf8(s).c_str()); }
std::string trim(std::string s) {
    auto first = s.find_first_not_of(" \t\r\n");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
}
SceneAssets::SceneAssets(std::shared_ptr<Installation> installation) : installation_(std::move(installation)) {
    for (const auto& [name, sources] : installation_->resources()) {
        (void)sources;
        if (name.rfind("proto/", 0) != 0 || name.size() < 10 || name.substr(name.size()-4)!=".pro") continue;
        auto filename = name.substr(6);
        unsigned id = 0;
        if (std::from_chars(filename.data(), filename.data() + filename.size(), id).ec == std::errc{})
            prototype_paths_[id] = name;
    }
}
const std::map<unsigned, std::string>& SceneAssets::table(const std::string& path) {
    auto found = tables_.find(path);
    if (found != tables_.end()) return found->second;
    std::map<unsigned, std::string> values;
    if (installation_->contains(path)) for (const auto& entry : read_messages(installation_->read(path)))
        values[entry.id] = trim(entry.text);
    return tables_.emplace(path, std::move(values)).first->second;
}
std::string SceneAssets::message(const std::string& path, unsigned id) {
    const auto& values = table(path);
    auto found = values.find(id);
    if (found == values.end()) throw std::runtime_error("Missing art mapping: " + path + " #" + std::to_string(id));
    return found->second;
}
std::string SceneAssets::resolve(std::uint32_t aid) {
    auto type = aid >> 28, num = (aid >> 19) & 511;
    if (type == 0) {
        unsigned outdoors = (aid >> 8) & 1;
        auto names = [&](unsigned n, bool flip) {
            unsigned group = (outdoors ? 0 : 200) + (flip ? 0 : 100);
            return message("art/tile/tilename.mes", group + n).substr(0, 3);
        };
        auto a = names((aid >> 22) & 63, aid & 128), b = names((aid >> 16) & 63, aid & 64);
        unsigned shape = (aid >> 12) & 15;
        const char* codes = "06b489237ea5dc10";
        char variant = 'a' + ((aid >> 9) & 7);
        if (shape == 15 || a == b) return "art/tile/" + a + "bse" + codes[shape] + variant + ".art";
        if (!shape) return "art/tile/" + b + "bse0" + variant + ".art";
        auto rank = [&](const std::string& name) {
            for (const auto& [id, value] : table("art/tile/tilename.mes"))
                if (id < 200 && value.substr(0, 3) == name) return int(id);
            return -1;
        };
        int ra = rank(a), rb = rank(b);
        if (ra < 0) return "art/tile/" + a + "bse" + codes[shape] + variant + ".art";
        if (rb < 0) return "art/tile/" + b + "bse" + codes[15-shape] + variant + ".art";
        return "art/tile/" + (ra < rb ? a+b : b+a) + codes[ra < rb ? shape : 15-shape] + variant + ".art";
    }
    if (type == 1) {
        static const char* pieces[] = {"bse","lfc","bse","bcl","bcr","tcl","tcr","uec","lec",
            "w3l","w3a","w3r","w4l","w4a","w4b","w4r","w5l","w5a","w5b","w5c","w5r",
            "d3l","d3a","d3r","d4l","d4a","d4b","d4r","d6l","d6a","d6b","d6c","d6d","d6r",
            "p3l","p3a","p3r","p4l","p4a","p4b","p4r","p5l","p5a","p5b","p5c","p5r"};
        std::istringstream structure(message("art/wall/structure.mes", (aid >> 20) & 255));
        std::string interior, exterior;
        structure >> interior >> exterior;
        unsigned rotation = (aid >> 11) & 7, piece = (aid >> 14) & 63;
        unsigned damage = aid & 0x480;
        if (rotation == 2 || rotation == 3 || rotation == 6 || rotation == 7)
            damage = ((damage & 0x400) ? 0x80 : 0) | ((damage & 0x80) ? 0x400 : 0);
        if (((damage & 0x400) && piece == 7) || ((damage & 0x80) && piece == 8)) piece = 0;
        if (piece >= std::size(pieces)) throw std::runtime_error("Unknown wall piece");
        char state = damage & 0x400 ? 'L' : damage & 0x80 ? (piece >= 2 && piece <= 6 ? 'L' : 'R') : 'U';
        auto prefix = (rotation / 2 == 0 || rotation / 2 == 3 ? interior : exterior).substr(0, 3);
        return "art/wall/" + prefix + pieces[piece] + state + char('0' + ((aid >> 8) & 3)) + ".art";
    }
    if (type == 3) {
        // TigArtPortalType: WINDOW = 0, DOOR = 1. Windows occupy MES 1001+.
        std::istringstream mapping(message("art/portal/portal.mes", num + ((aid & 0x400) ? 0 : 1001)));
        std::string filename; mapping >> filename; // Optional prototype number follows the filename.
        auto path = "art/portal/" + filename;
        if ((aid & 0x200) && path.size() >= 6) path[path.size()-6] = 'D';
        return path;
    }
    if (type == 4 || type == 7) {
        std::string group = type == 4 ? "scenery" : "container";
        return "art/" + group + "/" + message("art/" + group + "/" + group + ".mes", 1000*((aid >> 6)&31)+num);
    }
    if (type == 5) return "art/interface/" + message("art/interface/interface.mes", (aid >> 16) & 4095);
    if (type == 6) {
        auto subtype=(aid>>6)&15, itemType=aid&15, coverage=(aid>>14)&7;
        auto key=num+20*(subtype+50*itemType);
        if (itemType==2 && coverage!=0) key+=20*(5*coverage+10);
        const char* tables[]={"item_ground", "item_inven", "item_paper", "item_schematic"};
        return "art/item/"+message(std::string("art/item/")+tables[(aid>>12)&3]+".mes",key);
    }
    if(type==10)return "art/roof/"+message("art/roof/roofname.mes",num)+".art";
    if(type==11) {
        // Facade IDs split their asset number across bits 17..24 and 27.
        // Rugs use this category too; the low bits select a ground-cell frame.
        unsigned facade=((aid>>17)&255)|(((aid>>27)&1)<<8);
        return "art/facade/"+message("art/facade/facadename.mes",facade)+".art";
    }
    if(type==9)return "art/light/"+message("art/light/light.mes",(aid>>19)&511)+((aid&1)?"_s"+std::to_string(((aid>>4)&31)/8):"")+".art";
    if(type==2 || type==12 || type==13) {
        const char* bodies[]={"HM","DF","GH","HG","EF"};
        const char* armors[]={"UW","V1","LA","CM","PM","RB","PC","BN","CD"};
        unsigned armor=type==13?0:(aid>>20)&(type==12?7:15),body=(aid>>24)&7,gender=(aid>>27)&1,shield=(aid>>19)&1,weapon=aid&15,anim=(aid>>6)&31;
        if(armor>=std::size(armors) || weapon>=15 || (type==2 && body>=5))throw std::runtime_error("Invalid character art");
        if(type==2 && body==4 && gender==0)body=0;
        if(type==2 && (armor==4 || armor==6)){gender=1;if(body==4)body=0;if(body==2)body=1;}
        auto path=[&](unsigned animation,unsigned held,unsigned shielding,unsigned clothing) {
            std::string prefix,folder;
            if(type==2){prefix=std::string(bodies[body])+((clothing==4 || clothing==6 || animation==24)?'X':gender?'M':'F');folder="critter";}
            else {folder=type==12?"monster":"unique_npc";prefix=message("art/"+folder+"/"+folder+".mes",type==12?(aid>>23)&31:(aid>>20)&255);}
            auto code=std::string("ABCDEFGHIJKXYNZ")[held==7 && shielding?3:held];
            return "art/"+folder+"/"+prefix+"/"+prefix+(type==13?"":animation==24?"XX":armors[clothing])+(shielding?'S':'X')+code+char('a'+animation)+".art";
        };
        for(auto candidate:{path(anim,weapon,shield,armor),path(0,weapon,shield,armor),path(0,0,0,armor),path(0,0,0,0)})
            if(installation_->contains(resource_name(candidate)))return candidate;
        throw std::runtime_error("Character sprite not found");
    }
    throw std::runtime_error("Art category not yet rendered in sector preview: " + std::to_string(type));
}
Sprite SceneAssets::sprite(std::uint32_t aid) {
    if (auto cached = cache_.object(aid)) return *cached;
    Sprite result;
    result.path = resource_name(resolve(aid));
    Art art(installation_->read(result.path));
    unsigned type = aid >> 28;
    unsigned rotation = type == 0 || type == 5 || type == 6 || type==10 || type==11 ? 0 : type==9 ? ((aid>>9)&7) : ((aid >> 11) & 7);
    const unsigned direction = rotation;
    bool mirroredCharacter=(type==2 || type==12 || type==13) && rotation>0 && rotation<4;
    if(mirroredCharacter)rotation=8-rotation;
    if (art.rotations() == 1) rotation = 0;
    unsigned frame = type==9 ? ((aid>>12)&127) : type==11 ? ((aid>>1)&1023) : type == 0 || type == 1 || type == 6 ? 0 : type == 5 ? ((aid >> 8) & 255) : ((aid >> 14) & 31);
    if(type==11 && frame>=art.frames())throw std::runtime_error("Facade frame outside ART: "+result.path);
    if (frame >= art.frames()) frame = 0;
    unsigned palette = type==9 || type==11 ? 0 : (aid >> 4) & 3;
    if (!art.palette_available(palette)) palette = 0;
    auto bytes = art.rgba(rotation, frame, palette);
    const auto& info = art.frame(rotation, frame);
    QImage image(bytes.data(), info.width, info.height, info.width*4, QImage::Format_RGBA8888);
    // CE applies the AID mirror flag at blit time, independently of ART's
    // direction tables (which may all share a single stored rotation).
    bool flip = mirroredCharacter || ((aid & 1) && (type == 1 || type == 3 || type==10
        || (type == 0 && (aid & 128) && (aid & 64))));
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    result.pixels = QPixmap::fromImage(flip ? image.flipped(Qt::Horizontal) : image.copy());
#else
    result.pixels = QPixmap::fromImage(flip ? image.mirrored(true, false) : image.copy());
#endif
    result.hotspot = QPoint(info.hot_x, info.hot_y);
    if ((type == 1 || type == 3) && (direction < 2 || direction > 5)) result.hotspot += QPoint(-40,20);
    // tig_art_frame_data applies this after the wall/portal anchor adjustment.
    // Its two-pixel convention differs from generic image mirroring.
    if (flip) result.hotspot.setX(type==10?0:int(info.width) - result.hotspot.x() - (mirroredCharacter?1:2));
    cache_.insert(aid, new Sprite(result), std::max(1, int(bytes.size()/1024)));
    return result;
}
std::map<std::uint32_t,QString> SceneAssets::surfaces(bool roofs) {
    std::map<std::uint32_t,QString> result;
    if(roofs){const char* pieces[]={"Outer NW corner","West edge","North edge","Inner NW corner","Outer SW corner","Inner SW corner","Inner NE corner","Outer NE corner","Center","Outer SE corner","South edge","East edge","Inner SE corner"};
        for(const auto& [id,name]:table("art/roof/roofname.mes"))if(id<512)for(unsigned piece=0;piece<13;++piece){auto aid=(10u<<28)|(id<<19)|((piece>=9?piece-9:piece)<<14)|(piece>=9?0x11:0);result[aid]=display(name)+" · "+QString::fromUtf8(pieces[piece]);}
    }else for(const auto& [id,name]:table("art/tile/tilename.mes"))if(id<400 && id%100<64) {
        auto num=id%100;for(unsigned variant=0;variant<8;++variant){auto aid=(num<<22)|(num<<16)|((id<200?256:0)|(id%200<100?0xc0:0))|(variant<<9);
            try{auto path=resolve(aid);if(installation_->contains(path))result[aid]=QString(id<200?"Outdoor · ":"Indoor · ")+display(name)+QString(" · %1").arg(variant+1);}catch(const std::exception&){} }
    }
    return result;
}
bool SceneAssets::terrainBlocked(std::uint32_t aid) {
    if(aid==0xffffffff)return false;
    if((aid>>28)==11)return !(aid&1); // Facade walkability is stored in the AID.
    if(aid>>28)return false;
    unsigned id=((aid&256)?0:200)+((aid&128)?0:100)+((aid>>22)&63);
    const auto& names=table("art/tile/tilename.mes");auto found=names.find(id);if(found==names.end())return false;
    auto slash=found->second.find('/');if(slash==std::string::npos)return false;
    auto flags=found->second.substr(slash+1);flags=flags.substr(0,flags.find(' '));
    return flags.find_first_of("bf")!=std::string::npos;
}
GameObject SceneAssets::effective(const GameObject& object) {
    if (object.prototype) return object;
    auto found = prototypes_.find(object.prototype_id);
    if (found == prototypes_.end()) {
        auto path = prototype_paths_.find(object.prototype_id);
        if (path == prototype_paths_.end()) return object;
        std::vector<GameObject> objects;
        try{objects=read_objects(installation_->read(path->second));}
        catch(const std::exception& e){throw std::runtime_error(path->second+": "+e.what());}
        if (objects.size() != 1 || !objects[0].prototype || objects[0].type != object.type)
            throw std::runtime_error("Prototype and object type disagree");
        found = prototypes_.emplace(object.prototype_id, std::move(objects[0])).first;
    }
    auto result = object;
    result.fields = found->second.fields;
    for (const auto& field : object.fields) {
        auto old = std::find_if(result.fields.begin(), result.fields.end(), [&](const auto& f){ return f.name == field.name; });
        if (old != result.fields.end()) *old = field;
        else result.fields.push_back(field);
    }
    return result;
}
QString SceneAssets::description(const GameObject& object) {
    auto description = object.number("DESCRIPTION");
    for (const auto& path : {"mes/gamedesc.mes", "mes/description.mes"}) {
        const auto& values = table(path);
        auto found = values.find(unsigned(description));
        if (found != values.end()) return display(found->second);
    }
    return QString::fromLatin1(object_type_name(object.type)) + " #" + QString::number(object.prototype_id);
}

std::vector<CatalogEntry> SceneAssets::catalog() {
    std::vector<CatalogEntry> result;
    std::set<std::pair<unsigned,unsigned>> keys;
    catalogErrors.clear();
    auto add=[&](GameObject object,QString label,QString source) {
        auto aid=unsigned(object.number("CURRENT_AID"));
        if(!keys.emplace(object.prototype_id,aid).second) return;
        if(object.type==0) {
            const QString directions[]={"inside ↘","outside ↙","outside ↘","inside ↙"};
            label+=" · "+directions[((aid>>11)&7)/2];
        }
        result.push_back({std::move(object),std::move(label),std::move(source)});
    };
    std::vector<GameObject> walls;
    std::vector<GameObject> bases;
    for(const auto& [id,path] : prototype_paths_) {
        try {
            auto decoded=read_objects(installation_->read(path));
            if(decoded.size()!=1 || !decoded[0].prototype || decoded[0].prototype_id!=id) throw std::runtime_error("Invalid prototype");
            const auto& proto=decoded[0]; prototypes_[id]=proto;
            // Like obj_create_inst: a new sparse instance inherits prototype
            // behavior instead of copying the state of an existing world object.
            GameObject object; object.type=proto.type; object.prototype_id=id;
            set_number(object,"CURRENT_AID",proto.number("CURRENT_AID",proto.number("AID")));
            set_number(object,"AID",proto.number("AID",object.number("CURRENT_AID")));
            set_number(object,"FLAGS",proto.number("FLAGS"));
            if(object.type==16) set_number(object,"NPC_FLAGS",proto.number("NPC_FLAGS")|2);
            if(object.type==0) walls.push_back(object);
            bases.push_back(object);
            add(object,description(proto)+QString(" · #%1").arg(id),display(path));
        } catch(const std::exception& e) { catalogErrors.append(display(path)+": "+QString::fromUtf8(e.what())); }
    }
    // Resource tables contain many visual variants that do not have a dedicated
    // prototype (trees, beds, furniture, doors and containers).
    for(unsigned artType : {3u,4u,7u}) {
        std::string group=artType==3 ? "portal" : artType==4 ? "scenery" : "container";
        for(const auto& [key,mapping] : table("art/"+group+"/"+group+".mes")) {
            unsigned subtype=key/1000,number=key%1000;
            unsigned aid=0;
            if(artType==3) {
                number=key>=1001 ? key-1001 : key;
                if(number>511) continue;
                aid=(3u<<28)|(number<<19)|(key>=1001?0:0x400);
            } else {
                if(subtype>31 || number>511) continue;
                aid=(artType<<28)|(number<<19)|(subtype<<6);
            }
            unsigned type=artType==3 ? 1 : artType==7 ? 2 : subtype==0 ? 4 : 3;
            std::istringstream fields(mapping); std::string filename; unsigned explicitProto=0; fields>>filename>>explicitProto;
            const GameObject* base=nullptr;
            for(const auto& candidate : bases) if(candidate.type==type) {
                if(!base) base=&candidate;
                if(explicitProto && candidate.prototype_id==explicitProto) {base=&candidate;break;}
                auto original=unsigned(candidate.number("CURRENT_AID"));
                if(!explicitProto && (original>>28)==artType && ((original>>6)&31)==subtype) {base=&candidate;break;}
            }
            if(!base) continue;
            try {
                auto path=resource_name(resolve(aid)); if(!installation_->contains(path)) continue;
                auto object=*base; set_number(object,"CURRENT_AID",aid); set_number(object,"AID",aid);
                add(object,description(effective(object))+" · "+display(filename),display(path)+QString(" · prototype #%1").arg(object.prototype_id));
            } catch(const std::exception&) { }
        }
    }
    // Enumerate every available wall piece, structure, variation and face for
    // every wall prototype. A prototype can change collision and behavior even
    // when its art is identical, so it must not be substituted by another one.
    if(!walls.empty()) for(const auto& [structure,name] : table("art/wall/structure.mes")) {
        (void)name; if(structure>255) continue;
        for(unsigned piece=0;piece<46;piece++) for(unsigned direction : {0u,2u,4u,6u}) for(unsigned variation=0;variation<4;variation++) {
            unsigned aid=0x10000000u|(structure<<20)|(piece<<14)|(direction<<11)|(variation<<8)|((direction==2 || direction==6)?17:0);
            try {
                auto path=resource_name(resolve(aid)); if(!installation_->contains(path)) continue;
                for(const auto& wall:walls){auto object=wall; set_number(object,"CURRENT_AID",aid); set_number(object,"AID",aid);
                    if(piece==22) set_number(object,"FLAGS",object.number("FLAGS")|0x400);
                    auto label=description(effective(object))+QString(" · #%1 · structure %2 · piece %3 · variant %4").arg(object.prototype_id).arg(structure).arg(piece).arg(variation);
                    add(object,label,display(path));}
            } catch(const std::exception&) { /* Some structures have no opening frames. */ }
        }
    }
    // Invert the installed tile filenames and MES mappings, including terrain
    // transitions. Keep one palette entry per actual ART, never per open sector.
    struct TileName { unsigned number; bool outside,flip; };
    std::map<std::string,std::vector<TileName>> tileNames;
    for(const auto& [id,name] : table("art/tile/tilename.mes")) if(id<400 && id%100<64 && name.size()>=3)
        tileNames[resource_name(name.substr(0,3))].push_back({id%100,id<200,(id/100)%2==0});
    for(const auto& [path,sources] : installation_->resources()) {
        (void)sources;
        if(path.rfind("art/tile/",0)!=0 || path.size()!=21 || path.substr(17)!=".art") continue;
        auto filename=path.substr(9,8); auto first=tileNames.find(filename.substr(0,3));
        bool base=filename.substr(3,3)=="bse";
        auto second=base ? first : tileNames.find(filename.substr(3,3));
        if(first==tileNames.end() || second==tileNames.end() || filename[7]<'a' || filename[7]>'h') continue;
        std::string codes="06b489237ea5dc10"; auto shape=codes.find(filename[6]);
        if(shape==std::string::npos) continue;
        bool added=false;
        for(auto a : first->second) { for(auto b : second->second) {
            if(a.outside!=b.outside) continue;
            unsigned aid=(a.number<<22)|(b.number<<16)|(unsigned(shape)<<12)|(unsigned(filename[7]-'a')<<9)
                |(a.outside?256:0)|(a.flip?128:0)|(b.flip?64:0);
            try {
                if(resource_name(resolve(aid))!=path) continue;
                GameObject tile; set_number(tile,"CURRENT_AID",aid); tile.type=100;
                add(tile,"Ground · "+display(filename),display(path)); added=true; break;
            } catch(const std::exception&) { }
        } if(added) break; }
    }
    return result;
}
