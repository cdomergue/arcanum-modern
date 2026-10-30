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
        if (!scene.scripts().empty()) throw std::runtime_error("Tile/sector scripts cannot be exported as housing; use object scripts.");
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
        std::map<std::string, Bytes> files;
        std::ostringstream manifest, removed, tiles, roofs;
        manifest << "{0}{1 " << sx << ' ' << sy << "}\n";
        unsigned index = 100, removedIndex = 0;
        std::set<std::string> ids;
        for (auto o : objects) {
            auto loc = o.number("LOCATION");
            if ((std::uint32_t(loc) >> 6) != sx || (loc >> 38) != sy) throw std::runtime_error("Object outside the project sector");
            bool baseObject = false;
            for (const auto& b : original) if (same(o, b) && o.id == b.id) {baseObject = true; break;}
            if (baseObject) continue;
            if (o.id.size() != 36 || o.id.substr(0, 4) != "0200")
                throw std::runtime_error("Changed base object needs a new instance: duplicate it in WorldED, then remove the original.");
            if (!ids.insert(o.id).second) throw std::runtime_error("Duplicate object GUID");
            for (const auto& b : original) if (o.id == b.id)
                throw std::runtime_error("Existing campaign GUID cannot become a housing object; duplicate the object first.");
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
        auto add = [&](const char* name, const std::string& text) {files[name] = Bytes(text.begin(), text.end());};
        add("scene.mes", manifest.str()); add("removed.mes", removed.str());
        add("terrain.mes", tiles.str()); add("roofs.mes", roofs.str());
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
