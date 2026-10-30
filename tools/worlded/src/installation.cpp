#include "worlded/assets.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace worlded {
namespace fs = std::filesystem;
namespace {
std::string lower(std::string value) {
    for (char& c : value) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    return value;
}
fs::path child(const fs::path& root, const std::string& name) {
    if (fs::is_directory(root)) for (const auto& p : fs::directory_iterator(root))
        if (lower(p.path().filename().u8string()) == lower(name)) return p.path();
    return root / fs::u8path(name);
}
}
void Installation::add_archive(const fs::path& path) {
    auto archive = std::make_shared<Archive>(path);
    for (const auto& [name, entry] : archive->entries()) resources_[name].push_back({path, archive, entry.size});
}
void Installation::add_directory(const fs::path& path) {
    if (!fs::is_directory(path)) return;
    for (fs::recursive_directory_iterator it(path), end; it != end; ++it) {
        auto relative = it->path().lexically_relative(path);
        auto first = lower(relative.begin()->u8string());
        if (first == "save" || first == "tigcache" || first == "screenshots" || first[0] == '.') {
            if (it->is_directory()) it.disable_recursion_pending();
            continue;
        }
        // Do not traverse redirected folders or saved game files.
        if (it->is_symlink()) continue;
        if (!it->is_regular_file()) continue;
        auto name = resource_name(utf8_cp1252(relative.generic_u8string()));
        resources_[name].push_back({it->path(), {}, it->file_size()});
    }
}
Installation::Installation(const fs::path& game, const std::string& module) {
    if (fs::is_regular_file(game)) { add_archive(game); return; }
    if (!fs::is_directory(game)) throw std::runtime_error("Installation directory does not exist");
    auto tig = child(game, "tig.dat");
    if (fs::exists(tig)) add_archive(tig);
    // Root resources have lower priority than the game's DAT files in CE.
    for (const auto& folder : {"art", "proto", "mes", "rules", "scr", "dlg", "maps", "townmap", "worldmap"}) {
        auto directory = child(game, folder);
        if (!fs::is_directory(directory)) continue;
        for (fs::recursive_directory_iterator it(directory), end; it != end; ++it) {
            if (it->is_symlink() || !it->is_regular_file()) continue;
            auto name = resource_name(utf8_cp1252(it->path().lexically_relative(game).generic_u8string()));
            resources_[name].push_back({it->path(), {}, it->file_size()});
        }
    }
    std::map<std::string, fs::path> archives;
    for (const auto& entry : fs::directory_iterator(game)) {
        auto name = lower(entry.path().filename().u8string());
        if (name.rfind("arcanum", 0) == 0 && entry.path().extension() != ""
            && lower(entry.path().extension().string()) == ".dat") archives[name] = entry.path();
    }
    for (const auto& [name, path] : archives) add_archive(path);
    add_directory(child(game, "data"));
    // Match modern_uap.c activation markers and repository precedence.
    auto marker=[&](const fs::path& path,const std::string& expected) {
        if(!fs::is_regular_file(path)) return false;
        auto data=read_file(path); return std::string(data.begin(),data.end())==expected;
    };
    fs::path active,hotfix; bool french=false,hotfixFrench=false;
    bool manual=fs::exists(child(game,"Arcanum5.dat"));
    if(!manual) for(const auto& lang : {"fr","en"}) {
        auto candidate=game/"modern-ui"/(std::string("uap-")+lang)/"active";
        if(marker(candidate/"ready",std::string("2.0.2-")+lang+"-1\n")) {active=candidate; french=std::string(lang)=="fr"; break;}
    }
    if(!active.empty()) {
        for(const auto& name : {"arcanum5.dat","arcanumxambient.dat","arcanumxanims1base.dat","arcanumxanims2dupes.dat","arcanumxanims3uap.dat","arcanumxraces.dat"})
            add_archive(child(active/"global",name));
        if(french) add_archive(child(active,"french-global.dat"));
        add_directory(child(active,"data"));
    }
    for(const auto& lang : {"fr","en"}) {
        bool fr=std::string(lang)=="fr";
        if(!manual && (active.empty() || french!=fr)) continue;
        auto candidate=manual ? game/"modern-ui"/"uap-hotfix"/lang/"active" : active/"hotfix";
        if(marker(candidate/"ready",std::string("1.1.0-")+lang+"-1\n")
            && fs::is_regular_file(child(candidate,"Arcanum8.dat"))
            && fs::is_regular_file(child(candidate,"Arcanum.PATCH8"))
            && (!fr || fs::is_regular_file(child(candidate,"french-module.dat")))) {hotfix=candidate;hotfixFrench=fr;break;}
    }
    if(!hotfix.empty()) add_archive(child(hotfix,"Arcanum8.dat"));
    auto modules = child(game, "modules");
    auto base = child(modules, module + ".dat");
    if (fs::exists(base)) add_archive(base);
    std::map<unsigned long, fs::path> patches;
    if (fs::is_directory(modules)) for (const auto& entry : fs::directory_iterator(modules)) {
        auto name = lower(entry.path().filename().u8string());
        auto prefix = lower(module) + ".patch";
        if (name.rfind(prefix, 0) != 0) continue;
        auto number = name.substr(prefix.size());
        if (!number.empty() && number.size() < 9 && std::all_of(number.begin(), number.end(), [](char c) { return c >= '0' && c <= '9'; }))
            patches.emplace(std::stoul(number), entry.path());
    }
    for (const auto& [number, path] : patches) add_archive(path);
    if(!active.empty()) {
        if(lower(module)=="arcanum") {
            add_archive(child(active/"modules","arcanum.patch1"));
            add_archive(child(active/"modules","arcanum.patch9"));
            if(french) add_archive(child(active,"french-module.dat"));
        } else if(lower(module)=="vormantown") add_archive(child(active/"modules","vormantown.patch9"));
    }
    if(!hotfix.empty() && lower(module)=="arcanum") {
        add_archive(child(hotfix,"Arcanum.PATCH8"));
        if(hotfixFrench) add_archive(child(hotfix,"french-module.dat"));
    }
    add_directory(child(modules, module));
    if (resources_.empty()) throw std::runtime_error("No Arcanum resources found in this directory");
}
bool Installation::contains(const std::string& name) const { return resources_.find(resource_name(name)) != resources_.end(); }
Bytes Installation::read(const std::string& name) const {
    auto normalized = resource_name(name);
    auto it = resources_.find(normalized);
    if (it == resources_.end()) throw std::runtime_error("Missing resource: " + name);
    const auto& source = it->second.back();
    return source.archive ? source.archive->read(normalized) : read_file(source.file);
}
}
