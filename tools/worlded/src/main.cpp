#include "worlded/formats.hpp"
#include "worlded/assets.hpp"
#include "worlded/building.hpp"

#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace fs = std::filesystem;
using namespace worlded;

namespace {
// JSON output makes the format inspector usable by automation and tests.
std::string json(const std::string& text) {
    std::string result = "\"";
    static constexpr char hex[] = "0123456789abcdef";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) {
            result += "\\u00";
            result += hex[c >> 4];
            result += hex[c & 15];
        } else result += static_cast<char>(c);
    }
    return result + '"';
}

std::uint32_t number(std::string_view text) {
    int base = 10;
    if (text.substr(0, 2) == "0x") { text.remove_prefix(2); base = 16; }
    std::uint32_t value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value, base);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) throw std::runtime_error("Invalid unsigned integer");
    return value;
}

void inspect(const std::string& path, const Bytes& data) {
    const auto extension = fs::path(resource_name(path)).extension().string();
    if (extension == ".sec") {
        Sector sector(data);
        const auto& info = sector.info();
        std::cout << "{\"format\":\"sec\",\"version\":" << info.version
            << ",\"lights\":" << info.lights << ",\"tiles\":4096,\"roofs\":" << (info.roofs ? "true" : "false")
            << ",\"tile_scripts\":" << info.tile_scripts << ",\"townmap\":" << info.townmap
            << ",\"objects\":" << info.objects << ",\"object_bytes\":" << info.object_bytes
            << ",\"object_payload_validated\":false,\"lossless\":" << (sector.encode() == data ? "true" : "false") << "}\n";
    } else if (extension == ".scr") {
        const auto info = inspect_script(data);
        std::cout << "{\"format\":\"scr\",\"description\":" << json(cp1252_utf8(info.description))
            << ",\"lines\":" << info.lines << ",\"capacity\":" << info.capacity
            << ",\"flags\":" << info.flags << ",\"instructions_validated\":false}\n";
    } else if (extension == ".mes") {
        const auto messages = read_messages(data);
        std::cout << "{\"format\":\"mes\",\"entries\":[";
        bool first = true;
        for (const auto& message : messages) {
            if (!first) std::cout << ',';
            first = false;
            std::cout << "{\"id\":" << message.id << ",\"text\":" << json(cp1252_utf8(message.text)) << '}';
        }
        std::cout << "]}\n";
    } else throw std::runtime_error("Inspection supports .sec, .scr and .mes resources");
}

void usage() {
    std::cout << "WorldED Renaissance — format tools 0.3\n"
        "  worlded init PROJECT\n"
        "  worlded list ARCHIVE [PREFIX]\n"
        "  worlded inspect FILE\n"
        "  worlded inspect ARCHIVE ENTRY\n"
        "  worlded import ARCHIVE ENTRY PROJECT\n"
        "  worlded roundtrip SECTOR PROJECT\n"
        "  worlded tile SECTOR PROJECT X Y ART_ID\n"
        "  worlded audit ARCHIVE [PREFIX]\n"
        "  worlded decode FILE [ENTRY]\n"
        "  worlded pixels FILE ROTATION FRAME PALETTE\n"
        "  worlded audit-assets ARCHIVE [PREFIX]\n"
        "  worlded catalog GAME_OR_ARCHIVE\n"
        "  worlded read GAME_OR_ARCHIVE ENTRY\n"
        "  worlded house GAME NEW_PROJECT [WIDTH DEPTH CHESTS]\n"
        "Exports go to a separate workspace; existing files are never replaced.\n"
        "Use decode or audit-assets to validate object fields; references are not checked.\n";
}
void dump_objects(const std::vector<GameObject>& objects) {
    std::cout << "{\"objects\":[";
    bool first = true;
    for (const auto& obj : objects) {
        if (!first) std::cout << ',';
        first = false;
        std::cout << "{\"type\":" << obj.type << ",\"type_name\":" << json(object_type_name(obj.type))
            << ",\"prototype\":" << (obj.prototype ? "true" : "false") << ",\"prototype_id\":" << obj.prototype_id
            << ",\"id\":" << json(obj.id) << ",\"offset\":" << obj.offset << ",\"size\":" << obj.size << ",\"fields\":[";
        bool first_field = true;
        for (const auto& field : obj.fields) {
            if (!first_field) std::cout << ',';
            first_field = false;
            std::cout << "{\"name\":" << json(field.name) << ",\"kind\":" << json(field.kind)
                << ",\"present\":" << (field.present ? "true" : "false") << ",\"value\":" << json(object_field_text(field)) << '}';
        }
        std::cout << "]}";
    }
    std::cout << "]}\n";
}
void decode(const std::string& name, const Bytes& data) {
    auto ext = fs::path(resource_name(name)).extension();
    if (ext == ".sec") dump_objects(sector_objects(Sector(data)));
    else if (ext == ".mob" || ext == ".pro") dump_objects(read_objects(data));
    else if (ext == ".art") {
        Art art(data);
        std::cout << "{\"format\":\"art\",\"rotations\":" << art.rotations() << ",\"frames\":" << art.frames()
            << ",\"fps\":" << art.fps() << ",\"frame_data\":[";
        for (unsigned r = 0; r < art.rotations(); ++r) for (unsigned f = 0; f < art.frames(); ++f) {
            if (r || f) std::cout << ',';
            const auto& frame = art.frame(r, f);
            std::cout << "{\"width\":" << frame.width << ",\"height\":" << frame.height
                << ",\"hot_x\":" << frame.hot_x << ",\"hot_y\":" << frame.hot_y << '}';
        }
        std::cout << "]}\n";
    } else inspect(name, data);
}
void binary_output(const Bytes& data) {
#ifdef _WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    std::cout.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}
} // namespace

int main(int argc, char** argv) {
    try {
#ifdef _WIN32
        // The narrow Windows CRT argv depends on the user's code page. Obtain
        // UTF-8 from the Unicode command line so accented project paths work.
        int wide_count = 0;
        auto wide = CommandLineToArgvW(GetCommandLineW(), &wide_count);
        if (!wide) throw std::runtime_error("Cannot read Unicode command line");
        std::vector<std::string> arguments;
        for (int i = 0; i < wide_count; ++i) {
            const int size = WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, nullptr, 0, nullptr, nullptr);
            if (!size) { LocalFree(wide); throw std::runtime_error("Invalid command line encoding"); }
            std::string arg(static_cast<std::size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wide[i], -1, arg.data(), size, nullptr, nullptr);
            arg.pop_back();
            arguments.push_back(std::move(arg));
        }
        LocalFree(wide);
        std::vector<char*> pointers;
        for (auto& argument : arguments) pointers.push_back(argument.data());
        argc = wide_count;
        argv = pointers.data();
#endif
        if (argc < 2 || std::string_view(argv[1]) == "--help") { usage(); return argc < 2 ? 2 : 0; }
        const std::string command = argv[1];
        if (command == "pixels" && argc == 6) {
            binary_output(Art(read_file(fs::u8path(argv[2]))).rgba(number(argv[3]), number(argv[4]), number(argv[5])));
        } else if (command == "decode" && (argc == 3 || argc == 4)) {
            if (argc == 4) { auto name = utf8_cp1252(argv[3]); decode(name, Archive(fs::u8path(argv[2])).read(name)); }
            else decode(utf8_cp1252(fs::u8path(argv[2]).filename().u8string()), read_file(fs::u8path(argv[2])));
        } else if (command == "house" && (argc == 4 || argc == 7)) {
            BuildingPlan plan;
            if (argc == 7) { plan.width=number(argv[4]); plan.depth=number(argv[5]); plan.chests=number(argv[6]); }
            auto building=build_house(Installation(fs::u8path(argv[2])),plan);
            auto path=export_building(building,fs::u8path(argv[3]));
            std::cout << "{\"output\":" << json(path.u8string()) << ",\"objects\":" << building.objects.size() << "}\n";
        } else if (command == "catalog" && argc == 3) {
            Installation installation(fs::u8path(argv[2]));
            for (const auto& [name, sources] : installation.resources())
                std::cout << "{\"path\":" << json(cp1252_utf8(name)) << ",\"sources\":" << sources.size()
                    << ",\"source\":" << json(sources.back().file.u8string()) << "}\n";
        } else if (command == "read" && argc == 4) {
            binary_output(Installation(fs::u8path(argv[2])).read(utf8_cp1252(argv[3])));
        } else if (command == "audit-assets" && (argc == 3 || argc == 4)) {
            Archive archive(fs::u8path(argv[2]));
            const auto prefix = argc == 4 ? resource_name(utf8_cp1252(argv[3])) : "";
            std::size_t objects = 0, arts = 0, frames = 0, failed = 0, matched = 0;
            for (const auto& [name, entry] : archive.entries()) {
                (void)entry;
                if (name.compare(0, prefix.size(), prefix)) continue;
                auto ext = fs::path(name).extension();
                if (ext != ".sec" && ext != ".mob" && ext != ".pro" && ext != ".art") continue;
                ++matched;
                try {
                    if (ext == ".sec") objects += sector_objects(Sector(archive.read(name))).size();
                    else if (ext == ".mob" || ext == ".pro") objects += read_objects(archive.read(name)).size();
                    else if (ext == ".art") {
                        Art art(archive.read(name));
                        for (unsigned r = 0; r < art.rotations(); ++r) for (unsigned f = 0; f < art.frames(); ++f) {
                            art.rgba(r, f); ++frames;
                        }
                        ++arts;
                    }
                } catch (const std::exception& error) { ++failed; std::cerr << name << ": " << error.what() << '\n'; }
            }
            std::cout << "{\"objects\":" << objects << ",\"art_files\":" << arts << ",\"frames\":" << frames << ",\"failures\":" << failed << "}\n";
            return failed || !matched ? 1 : 0;
        } else if (command == "init" && argc == 3) {
            create_project(fs::u8path(argv[2]));
            std::cout << "{\"created\":true}\n";
        } else if (command == "list" && (argc == 3 || argc == 4)) {
            Archive archive(fs::u8path(argv[2]));
            const std::string prefix = argc == 4 ? resource_name(utf8_cp1252(argv[3])) : "";
            for (const auto& [name, entry] : archive.entries()) {
                if (name.compare(0, prefix.size(), prefix) == 0)
                    std::cout << "{\"path\":" << json(cp1252_utf8(name)) << ",\"bytes\":" << entry.size << "}\n";
            }
        } else if (command == "inspect" && (argc == 3 || argc == 4)) {
            if (argc == 4) {
                const auto name = utf8_cp1252(argv[3]);
                inspect(name, Archive(fs::u8path(argv[2])).read(name));
            } else inspect(utf8_cp1252(fs::u8path(argv[2]).filename().u8string()), read_file(fs::u8path(argv[2])));
        } else if (command == "import" && argc == 5) {
            Archive archive(fs::u8path(argv[2]));
            const auto name = utf8_cp1252(argv[3]);
            const auto output = export_resource(fs::u8path(argv[4]), "imports/" + resource_name(name), archive.read(name));
            std::cout << "{\"output\":" << json(output.u8string()) << "}\n";
        } else if ((command == "roundtrip" && argc == 4) || (command == "tile" && argc == 7)) {
            const auto input = fs::u8path(argv[2]);
            Sector sector(read_file(input));
            if (command == "tile") sector.set_tile(number(argv[4]), number(argv[5]), number(argv[6]));
            const auto output = export_resource(fs::u8path(argv[3]), "exports/" + utf8_cp1252(input.filename().u8string()), sector.encode());
            std::cout << "{\"output\":" << json(output.u8string()) << "}\n";
        } else if (command == "audit" && (argc == 3 || argc == 4)) {
            Archive archive(fs::u8path(argv[2]));
            const std::string prefix = argc == 4 ? resource_name(utf8_cp1252(argv[3])) : "";
            std::size_t sectors = 0, scripts = 0, failures = 0;
            std::uint64_t objects = 0, bytes = 0;
            for (const auto& [name, entry] : archive.entries()) {
                if (name.compare(0, prefix.size(), prefix) != 0) continue;
                const auto ext = fs::path(name).extension();
                if (ext != ".sec" && ext != ".scr") continue;
                try {
                    const auto data = archive.read(name);
                    if (ext == ".sec") {
                        Sector sector(data);
                        if (sector.encode() != data) throw std::runtime_error("SEC roundtrip changed bytes");
                        objects += sector.info().objects;
                        ++sectors;
                    } else { inspect_script(data); ++scripts; }
                    bytes += entry.size;
                } catch (const std::exception& error) {
                    ++failures;
                    std::cerr << json(cp1252_utf8(name)) << ": " << error.what() << '\n';
                }
            }
            std::cout << "{\"sectors\":" << sectors << ",\"scripts\":" << scripts << ",\"objects_preserved\":" << objects
                << ",\"bytes\":" << bytes << ",\"failures\":" << failures
                << ",\"object_payload_validated\":false,\"instructions_validated\":false}\n";
            if (!sectors && !scripts && !failures) throw std::runtime_error("No supported resources matched the audit");
            return failures ? 1 : 0;
        } else { usage(); return 2; }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "worlded: " << error.what() << '\n';
        return 1;
    }
}
