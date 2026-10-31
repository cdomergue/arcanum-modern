#include "../worlded/src/inventory.hpp"
// Convert a saved WorldED project into additive housing resources.
#include "worlded/world.hpp"
#include "worlded/building.hpp"
#include <QtCore>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
using namespace worlded;
namespace fs = std::filesystem;

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    try {
        const auto args = app.arguments();
        if (args.size() < 4) {
            std::cerr << "Usage: worlded-house-export GAME PROJECT NEW_OUTPUT [--guard GUID]...\n";
            return 2;
        }
        auto path = [](const QString& s) { return fs::u8path(s.toUtf8().constData()); };
        const auto project = path(args[2]), output = path(args[3]);
        if (fs::exists(output)) throw std::runtime_error("Output already exists; choose a new directory.");
        std::set<std::string> guards;
        for (int i = 4; i < args.size(); i += 2) {
            if (args[i] != "--guard" || i + 1 >= args.size()) throw std::runtime_error("Expected --guard GUID");
            guards.insert(args[i + 1].toLower().toStdString());
        }
        QFile file(QString::fromStdString((project / "building.json").u8string()));
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read project building.json");
        QJsonParseError error;
        auto doc = QJsonDocument::fromJson(file.readAll(), &error);
        auto info = doc.object();
        if (error.error != QJsonParseError::NoError || info["format"] != "worlded-scene"
            || !info["existing_map"].toBool()) throw std::runtime_error("Choose a saved project on an existing world map.");
        auto map = info["map"].toString().toStdString();
        // The housing runtime currently operates on Arcanum's main map.
        if (resource_name(map) != "arcanum1-024-fixed") throw std::runtime_error("Housing requires Arcanum's main map.");
        double number = info["sector"].toDouble(-1);
        if (number < 0 || number >= double(2000ull << 26) || number != double(std::uint64_t(number)))
            throw std::runtime_error("Invalid project sector");
        auto sector = std::uint64_t(number);
        unsigned sx = sector & 0x3ffffff, sy = sector >> 26;
        if (sx >= 2000 || sy >= 2000) throw std::runtime_error("Invalid housing sector");
        Installation game(path(args[1]));
        auto baseline = read_world_sector(game, map, sx, sy);
        auto input = project / "exports" / "maps" / map;
        Sector scene(read_file(input / (std::to_string(sector) + ".sec")));
        auto settings=scene.settings(),baseSettings=baseline.sector.settings();
        auto original = sector_objects(baseline.sector);
        for (auto& o : original) if (baseline.generated) {
            auto loc = o.number("LOCATION");
            set_number(o, "LOCATION", (sx * 64ull + (loc & 63)) | ((sy * 64ull + ((loc >> 32) & 63)) << 32));
        }
        auto objects = sector_objects(scene);
        for (const auto& f : fs::directory_iterator(input)) if (f.path().extension() == ".mob") {
            auto mobile = read_objects(read_file(f.path()));
            objects.insert(objects.end(), mobile.begin(), mobile.end());
        }
        auto same = [](const GameObject& a, const GameObject& b) {
            return a.prototype_id == b.prototype_id && a.number("LOCATION") == b.number("LOCATION")
                && a.number("CURRENT_AID") == b.number("CURRENT_AID") && a.number("OFFSET_X") == b.number("OFFSET_X")
                && a.number("OFFSET_Y") == b.number("OFFSET_Y");
        };
        auto baseLights=baseline.sector.lights(),newLights=scene.lights();
        std::vector<Bytes> addedLights,removedLights;
        for(auto light:newLights){auto found=std::find(baseLights.begin(),baseLights.end(),light);if(found==baseLights.end())addedLights.push_back(light);else baseLights.erase(found);}
        removedLights=baseLights;
        GameObject lightPrototype;
        if(!addedLights.empty()){
            bool found=false;for(const auto& [name,sources]:game.resources())if(name.rfind("proto/",0)==0 && fs::path(name).extension()==".pro"){auto candidates=read_objects(game.read(name));if(!candidates.empty() && candidates.front().type==3){lightPrototype=candidates.front();found=true;break;}}
            if(!found)throw std::runtime_error("A scenery prototype is required for housing lights");
        }
        unsigned lightIndex=0;
        for(const auto& light:addedLights){
            auto get=[&](int offset){return std::uint32_t(light[offset])|(std::uint32_t(light[offset+1])<<8)|(std::uint32_t(light[offset+2])<<16)|(std::uint32_t(light[offset+3])<<24);};
            if(get(0) || get(4))throw std::runtime_error("Only independent sector lights can be exported");
            GameObject proxy;proxy.type=3;proxy.prototype_id=lightPrototype.prototype_id;
            QByteArray key="WorldED housing light v1:"+QByteArray::number(sector)+":"+QByteArray::number(lightIndex++);key.append(reinterpret_cast<const char*>(light.data()),light.size());
            proxy.id="0200"+QCryptographicHash::hash(key,QCryptographicHash::Sha256).left(16).toHex().toStdString();
            auto loc=std::uint64_t(get(8))|(std::uint64_t(get(12))<<32);
            set_number(proxy,"LOCATION",(sx*64ull+(loc&63))|((sy*64ull+((loc>>32)&63))<<32));
            set_number(proxy,"FLAGS",0x2500);set_number(proxy,"SCENERY_FLAGS",0);set_number(proxy,"CURRENT_AID",0x40000000);set_number(proxy,"AID",0x40000000);
            set_number(proxy,"OFFSET_X",get(16));set_number(proxy,"OFFSET_Y",get(20));set_number(proxy,"LIGHT_FLAGS",get(24)&~0x80u);set_number(proxy,"LIGHT_AID",get(28));
            set_number(proxy,"LIGHT_COLOR",(unsigned(light[32])<<16)|(unsigned(light[34])<<8)|light[33]);
            ObjectField scripts;scripts.name="SCRIPTS_IDX";scripts.kind="SCRIPT_ARRAY";scripts.present=true;proxy.fields.push_back(scripts);
            ObjectField overlays;overlays.name="OVERLAY_LIGHT_AID";overlays.kind="UINT32_ARRAY";overlays.present=true;for(unsigned n=0;n<4;n++)overlays.elements.push_back({n,Bytes(4,255)});proxy.fields.push_back(overlays);
            objects.push_back(std::move(proxy));
        }
        std::map<std::string, Bytes> files;
        std::ostringstream manifest, removed, tiles, roofs, blocks, environment, scripts;
        manifest << "{0}{1 " << sx << ' ' << sy << "}\n";
        unsigned index = 100, removedIndex = 0;
        std::ostringstream inventory;unsigned inventoryIndex=0;
        std::set<std::string> ids;
        for (auto o : objects) {
            auto loc = o.number("LOCATION");
            if ((std::uint32_t(loc) >> 6) != sx || (loc >> 38) != sy) throw std::runtime_error("Object outside the project sector");
            bool baseObject = false;
            for (const auto& b : original) if (same(o, b) && o.id == b.id) {
                if(encode_instance(o)!=encode_instance(b))throw std::runtime_error("Changed campaign object must be duplicated before housing export; original properties cannot be replaced safely.");
                baseObject = true; break;}
            if (baseObject) continue;
            if (o.id.size() != 36 || o.id.substr(0, 4) != "0200")
                throw std::runtime_error("Changed base object needs a new instance: duplicate it in WorldED, then remove the original.");
            if (!ids.insert(o.id).second) throw std::runtime_error("Duplicate object GUID");
            for (const auto& b : original) if (o.id == b.id)
                throw std::runtime_error("Existing campaign GUID cannot become a housing object; duplicate the object first.");
            auto parent=inventory_parent(o);
            if(o.number("FLAGS")&0x1000u){
                if(parent.empty() || !inventory_item(o))throw std::runtime_error("Inventory object has no valid parent");
                auto owner=std::find_if(objects.begin(),objects.end(),[&](const auto& candidate){return candidate.id==parent;});
                if(owner==objects.end() || !inventory_owner(*owner))throw std::runtime_error("Inventory owner is missing from the project");
                if(std::any_of(original.begin(),original.end(),[&](const auto& candidate){return candidate.id==parent;}))throw std::runtime_error("Housing cannot initialize inventory on an existing campaign owner");
                auto children=inventory_ids(*owner);if(std::count(children.begin(),children.end(),o.id)!=1)throw std::runtime_error("Inventory parent does not reference its child exactly once");
                inventory<<"{"<<inventoryIndex++<<"}{"<<o.id<<" "<<parent<<"}\n";
            }else if(!parent.empty() && parent.substr(0,4)=="0200")throw std::runtime_error("Item parent requires the inventory flag");
            if(inventory_owner(o) && o.number(o.type==2?"CONTAINER_INVENTORY_NUM":"CRITTER_INVENTORY_NUM"))inventory_contents(objects,o);
            std::string role = o.type == 1 ? "portal" : o.type == 2 ? "chest" : "object";
            if (guards.erase(o.id)) {
                if (o.type != 16) throw std::runtime_error("A guard must be an NPC instance");
                role = "guard";
            }
            // Keep new objects out of the base sector's saved DIF ordering.
            set_number(o, "FLAGS", o.number("FLAGS") | 0x2000);
            files["objects/" + o.id + ".mob"] = encode_instance(o);
            manifest << '{' << index++ << "}{" << role << ' ' << o.id << "}\n";
        }
        if (!guards.empty()) throw std::runtime_error("Guard GUID not found among new project objects");
        if (index == 100) throw std::runtime_error("Project has no new housing objects");
        for (const auto& b : original) {
            bool retained = false;
            for (const auto& a : objects) if (same(a, b) && !(a.number("FLAGS") & (2u | 0x100u))) {retained = true; break;}
            if (!retained) removed << '{' << removedIndex++ << "}{" << b.number("LOCATION") << ' '
                << b.number("CURRENT_AID") << ' ' << std::int32_t(b.number("OFFSET_X")) << ' '
                << std::int32_t(b.number("OFFSET_Y")) << "}\n";
        }
        for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 64; ++x)
            if (scene.tile(x, y) != baseline.sector.tile(x, y)) tiles << '{' << y * 64 + x << "}{" << scene.tile(x, y) << "}\n";
        for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 16; ++x)
            if (scene.roof(x, y) != baseline.sector.roof(x, y)) roofs << '{' << y * 16 + x << "}{" << scene.roof(x, y) << "}\n";
        for(unsigned y=0;y<64;y++)for(unsigned x=0;x<64;x++)
            if(scene.blocked(x,y)!=baseline.sector.blocked(x,y))blocks << '{' << y*64+x << "}{" << scene.blocked(x,y) << "}\n";
        if(settings.aptitude!=baseSettings.aptitude)environment << "{0}{" << settings.aptitude << "}\n";
        if(settings.light!=baseSettings.light)environment << "{1}{" << settings.light << "}\n";
        if(settings.music!=baseSettings.music)environment << "{2}{" << settings.music << "}\n";
        if(settings.ambient!=baseSettings.ambient)environment << "{3}{" << settings.ambient << "}\n";
        std::map<std::uint32_t,SectorScript> beforeScripts,afterScripts;
        for(auto s:baseline.sector.scripts())beforeScripts[s.tile]=s;
        for(auto s:scene.scripts())afterScripts[s.tile]=s;
        for(auto [tile,s]:beforeScripts)if(!afterScripts.count(tile))afterScripts[tile]={tile,0,0,0};
        for(auto [tile,s]:afterScripts){auto old=beforeScripts.find(tile);
            if(old!=beforeScripts.end() && old->second.number==s.number && old->second.flags==s.flags && old->second.counters==s.counters)continue;
            scripts << '{' << (tile==UINT32_MAX?4096:tile) << "}{" << s.number << ' ' << s.flags << ' ' << s.counters << "}\n";
        }
        auto add = [&](const char* name, const std::string& text) {files[name] = Bytes(text.begin(), text.end());};
        add("scene.mes", manifest.str()); add("removed.mes", removed.str());
        add("terrain.mes", tiles.str()); add("roofs.mes", roofs.str());
        if(inventoryIndex){auto text=inventory.str();files["inventory.mes"]=Bytes(text.begin(),text.end());}
        if(!removedLights.empty()){Bytes data(4,0);auto count=unsigned(removedLights.size());for(int b=0;b<4;b++)data[b]=count>>(8*b);for(const auto& light:removedLights)data.insert(data.end(),light.begin(),light.end());files["removed-lights.bin"]=std::move(data);}
        add("blocking.mes",blocks.str());add("environment.mes",environment.str());add("scripts.mes",scripts.str());
        // Optional clearance rules belong to this project, not a particular property.
        if (fs::exists(project / "vegetation.mes")) files["vegetation.mes"] = read_file(project / "vegetation.mes");
        // Preserve project-owned scripts/dialogues; the content builder mounts these at the resource root.
        for (const auto& group : {"scr", "dlg", "proto"}) {
            auto dir = project / "exports" / group;
            if (!fs::exists(dir)) continue;
            for (const auto& entry : fs::directory_iterator(dir)) {
                if (entry.is_symlink() || !entry.is_regular_file()) throw std::runtime_error("Invalid project resource");
                files[std::string("resources/") + group + "/" + entry.path().filename().u8string()] = read_file(entry.path());
            }
        }
        for (const auto& [name, data] : files) {
            fs::create_directories((output / name).parent_path());
            std::ofstream f(output / name, std::ios::binary);
            f.write(reinterpret_cast<const char*>(data.data()), data.size());
            if (!f) throw std::runtime_error("Cannot write export");
        }
        std::cout << index - 100 << " objects exported for sector " << sx << ", " << sy << "\n";
    } catch (const std::exception& e) {std::cerr << e.what() << '\n'; return 1;}
}
