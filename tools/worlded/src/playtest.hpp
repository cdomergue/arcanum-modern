#pragma once
#include "worlded/npc_routes.hpp"
#include <charconv>
#include "worlded/building.hpp"
#include <QtCore>
#include <fstream>
#include <set>

namespace worlded {
// Local machine preference, separate from portable scene/mod exports.
inline QString project_installation(const QString& project) {
    QFile file(QDir(project).filePath("worlded.local.json"));
    if(!file.exists())return {};
    if(QFileInfo(file).isSymLink() || !file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read local project settings");
    QJsonParseError error;auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject() || doc.object().value("format")!="worlded-local" || !doc.object().value("installation").isString())throw std::runtime_error("Invalid local project settings");
    return doc.object().value("installation").toString();
}
inline void remember_project_installation(const QString& project,const QString& installation) {
    auto projectPath=QFileInfo(project).canonicalFilePath(),source=QFileInfo(installation).canonicalFilePath();
    if(projectPath.isEmpty() || source.isEmpty())throw std::runtime_error("Cannot resolve project or installation path");
    auto root=QFileInfo(source).isDir()?source:QFileInfo(source).canonicalPath();
#ifdef Q_OS_WIN
    auto relation=QDir(root).relativeFilePath(projectPath).toLower();
#else
    auto relation=QDir(root).relativeFilePath(projectPath);
#endif
    if(relation=="." || (!relation.startsWith("../") && relation!=".." && !QDir::isAbsolutePath(relation)))throw std::runtime_error("Project settings cannot be written inside the source installation");
    auto target=QDir(projectPath).filePath("worlded.local.json");if(QFileInfo(target).isSymLink())throw std::runtime_error("Local project settings cannot be a symbolic link");
    QJsonObject settings{{"format","worlded-local"},{"version",1},{"installation",source}};
    QSaveFile file(target);auto data=QJsonDocument(settings).toJson(QJsonDocument::Indented);
    if(!file.open(QIODevice::WriteOnly) || file.write(data)!=data.size() || !file.commit())throw std::runtime_error("Cannot save local project settings");
}
inline void write_bytes(const std::filesystem::path& path,const Bytes& data) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path,std::ios::binary); file.write(reinterpret_cast<const char*>(data.data()),data.size());
    if(!file) throw std::runtime_error("Cannot write "+path.u8string());
}
// TIG paths are case insensitive even on Linux. Preserve the existing spelling
// and remove shadow copies inside the NEW test installation only. Otherwise a
// lowercase mobile bundle coexists with Arcanum's capitalized obfuscated name.
inline std::filesystem::path test_resource_path(const std::filesystem::path& root,const std::filesystem::path& relative) {
    namespace fs=std::filesystem;auto target=root;
    for(auto part=relative.begin();part!=relative.end();++part){
        if(*part==".." || part->is_absolute())throw std::runtime_error("Invalid test resource path");
        auto name=QString::fromStdString(part->u8string());std::vector<fs::path> matches;
        if(fs::is_directory(target))for(const auto& entry:fs::directory_iterator(target))if(QString::fromStdString(entry.path().filename().u8string()).compare(name,Qt::CaseInsensitive)==0)matches.push_back(entry.path());
        bool last=std::next(part)==relative.end();
        if(matches.empty()){target/=*part;continue;}
        std::sort(matches.begin(),matches.end());target=matches.front();
        if(fs::is_symlink(target))throw std::runtime_error("Redirected test resource");
        if(matches.size()>1){if(!last)throw std::runtime_error("Duplicate directory casing in test installation: "+target.string());
            for(std::size_t i=1;i<matches.size();i++){if(!fs::is_regular_file(matches[i]) || fs::is_symlink(matches[i]))throw std::runtime_error("Invalid duplicate test resource");fs::remove(matches[i]);}}
    }
    return target;
}
inline QJsonObject project_json(const std::filesystem::path& path) {
    auto data=read_file(path); QJsonParseError error;
    auto document=QJsonDocument::fromJson(QByteArray(reinterpret_cast<const char*>(data.data()),data.size()),&error);
    if(error.error!=QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid project JSON");
    return document.object();
}
inline Building load_project(const std::filesystem::path& path) {
    namespace fs=std::filesystem;
    auto info=project_json(path/"building.json"); auto map=info["map"].toString().toStdString();
    if(info["format"]!="worlded-scene" || map.empty() || map=="." || map==".." || map.find_first_of("/\\")!=std::string::npos) throw std::runtime_error("Invalid scene project");
    auto id=std::uint64_t(info["sector"].toDouble());
    auto folder=path/"exports"/"maps"/fs::u8path(map);
    Building result{BuildingPlan{},Sector(read_file(folder/(std::to_string(id)+".sec"))),{}, {}};
    result.map_name=map;result.sector_id=id;result.existing_map=info["existing_map"].toBool();
    result.plan.x=info["x"].toInt(24);result.plan.y=info["y"].toInt(24);
    result.plan.width=info["width"].toInt(12);result.plan.depth=info["depth"].toInt(10);
    result.objects=sector_objects(result.ground);
    for(const auto& entry:fs::directory_iterator(folder)) if(entry.path().extension()==".mob") {
        auto objects=read_objects(read_file(entry.path()));result.objects.insert(result.objects.end(),objects.begin(),objects.end());
    }
    for(auto& object:result.objects) {
        auto loc=object.number("LOCATION");
        if((std::uint32_t(loc)>>6)!=(id&0x3ffffff) || (loc>>38)!=(id>>26)) throw std::runtime_error("Project object outside its sector");
        set_number(object,"LOCATION",(loc&63)|(((loc>>32)&63)<<32)); object.editor_id=clone_instance(object).id;
        transform_npc_routes(object,id,false);
    }
    if(fs::exists(path/"location-names.json")) { auto names=project_json(path/"location-names.json");
        for(auto it=names.begin();it!=names.end();++it) result.location_names[it.key().toStdString()]=it.value().toString().toStdString();
    }
    auto proto=path/"exports"/"proto";
    if(fs::exists(proto)) for(const auto& entry:fs::directory_iterator(proto)) if(entry.is_regular_file()) result.prototypes["proto/"+entry.path().filename().string()]=read_file(entry.path());
    for(const auto& group:{"scr","dlg","script-sources"}){auto folder=std::string(group)=="script-sources"?path/group:path/"exports"/group;if(!fs::exists(folder))continue;
        if(fs::is_symlink(folder))throw std::runtime_error("Redirected content folder");
        for(const auto& entry:fs::directory_iterator(folder)){if(entry.is_symlink() || !entry.is_regular_file())throw std::runtime_error("Invalid project content file");auto key=std::string(group)+"/"+entry.path().filename().string();result.content[resource_name(key)]=read_file(entry.path());}}
    return result;
}
inline std::string mobile_bundle_name(std::string map) {
    for(char& c:map) if((c>='A' && c<='Z') || (c>='a' && c<='z')) {int end=c<='Z'?'Z':'z';int shifted=int(c)+13;if(shifted>end)shifted-=24;c=char(shifted);}
    std::reverse(map.begin(),map.end()); return "maps/"+map;
}
// Refuse edits to sectors already cached in this save until diff migration is supported.
inline void check_save_sector(const std::filesystem::path& index,const Building& scene) {
    auto data=read_file(index);std::size_t cursor=0;int depth=0;bool ended=false;std::uint64_t total=0;
    auto u32=[&]() {if(cursor+4>data.size()) throw std::runtime_error("Truncated save index"); std::uint32_t n=0;for(int i=0;i<4;i++)n|=std::uint32_t(data[cursor++])<<(8*i);return n;};
    while(cursor<data.size()) {
        auto type=u32();if(type==3) {ended=true;break;} if(type==2) {if(--depth<0) throw std::runtime_error("Invalid save directory");continue;}
        if(type>1) throw std::runtime_error("Unknown save index record");
        auto size=u32();if(size>4096 || cursor+size>data.size()) throw std::runtime_error("Invalid save path");
        std::string name(data.begin()+cursor,data.begin()+cursor+size);cursor+=size;
        if(name.find(std::to_string(scene.sector_id))!=std::string::npos) throw std::runtime_error("This sector already exists in the save. Choose another save or an unvisited sector.");
        if(type==0) total+=u32();else depth++;
    }
    auto payload=index;payload.replace_extension(".tfaf");
    if(!ended || depth || total!=std::filesystem::file_size(payload)) throw std::runtime_error("Invalid save archive lengths");
}
struct PlaytestResult {std::filesystem::path root,game,executable,project;};
inline bool supports_worlded_test(const QString& path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly))return false;
    auto header=file.peek(4);
#ifdef Q_OS_WIN
    if(!header.startsWith("MZ"))return false;
#elif defined(Q_OS_LINUX)
    if(header!=QByteArray("\x7f" "ELF",4))return false;
#else
    if(header.startsWith("#!"))return false;
#endif
    QByteArray tail;bool argument=false,marker=false;
    while(!file.atEnd()) {auto block=tail+file.read(65536);argument|=block.contains("--worlded-test");marker|=block.contains(".worlded-playtest");
        if(argument && marker)return true;
        tail=block.right(32);}
    return false;
}
inline void configure_test_process(QProcess& process,const std::filesystem::path& root) {
    auto config=project_json(root/"playtest.json");auto exe=config["executable"].toString(),slot=config["slot"].toString();
    int x=config["x"].toInt(-1),y=config["y"].toInt(-1);
    if(config["format"]!="worlded-playtest" || exe.isEmpty() || exe=="." || exe==".." || exe.contains('/') || exe.contains('\\')
        || !QRegularExpression("^Slot[0-9A-Z]{4}$").match(slot).hasMatch() || x<0 || x>=2000 || y<0 || y>=2000
        || read_file(root/"game/.worlded-playtest")!=Bytes({'1','\n'}))throw std::runtime_error("Invalid WorldED test copy.");
    auto program=QString::fromStdString((root/"game"/std::filesystem::u8path(exe.toStdString())).u8string());
    if(!supports_worlded_test(program))throw std::runtime_error("This engine does not support WorldED. Recreate the test with a compatible Renaissance executable.");
    process.setProgram(program);process.setWorkingDirectory(QString::fromStdString((root/"game").u8string()));
    process.setArguments({"--worlded-test",slot,QString::number(x),QString::number(y)});
    auto environment=QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_LINUX
    environment.insert("SDL_VIDEO_DRIVER","x11");environment.insert("SDL_RENDER_DRIVER","software");environment.insert("SDL_FRAMEBUFFER_ACCELERATION","0");
#endif
    process.setProcessEnvironment(environment);process.setProcessChannelMode(QProcess::MergedChannels);
    process.setStandardOutputFile(QString::fromStdString((root/"test-launch.log").u8string()));
}
inline void copy_test_tree(const std::filesystem::path& from,const std::filesystem::path& to) {
    namespace fs=std::filesystem;
    fs::create_directories(to);
    // NTFS directories created by WSL may contain both Proto and proto. Merge
    // them on every platform, in source enumeration order (the resource index
    // uses that same order and its last loose-file entry wins).
    std::map<QString,fs::path> children;
    for(const auto& entry:fs::directory_iterator(to))children[QString::fromStdString(entry.path().filename().u8string()).toLower()]=entry.path();
        for(const auto& entry:fs::directory_iterator(from)) {
            auto filename=entry.path().filename(); auto lower=QString::fromStdString(filename.string()).toLower();
            if(lower=="save" || lower=="tigcache" || lower=="screenshots" || lower=="local-maps" || lower.startsWith('.')) continue;
            if(from.filename()=="ArcanumModern" && entry.is_directory() && !QStringList{"lib","licenses","uap","docs"}.contains(lower)) continue;
            auto found=children.find(lower);auto target=found==children.end()?to/filename:found->second;
            if(fs::is_symlink(target))throw std::runtime_error("Redirected test resource: "+target.string());
            children[lower]=target;
            if(entry.is_directory()) {if(entry.is_symlink()) throw std::runtime_error("Redirected source folder: "+entry.path().string());fs::create_directories(target);copy_test_tree(entry.path(),target);}
            else if(entry.is_regular_file()) {fs::create_directories(to);fs::copy_file(entry.path(),target,fs::copy_options::overwrite_existing);}
        }
}

inline PlaytestResult create_playtest(const Building& scene,const std::filesystem::path& source,
    const std::filesystem::path& saveIndex,const std::filesystem::path& executable,
    const std::filesystem::path& destination,const std::string& language) {
    namespace fs=std::filesystem;
    if(!scene.existing_map || scene.map_name!="arcanum1-024-fixed") throw std::runtime_error("Testing a save requires a sector on the main map.");
    if(!fs::is_regular_file(executable) || saveIndex.extension()!=".tfai") throw std::runtime_error("Choose an executable and a .tfai save index.");
    if(!supports_worlded_test(QString::fromStdString(executable.u8string())))throw std::runtime_error("This executable does not support WorldED. Choose a compatible Renaissance engine, not the .sh launcher or the original game executable.");
    if(!QRegularExpression("^Slot[0-9A-Z]{4}$").match(QString::fromStdString(saveIndex.stem().string())).hasMatch()) throw std::runtime_error("Invalid save slot name");
    check_save_sector(saveIndex,scene);
    Installation installation(source);
    auto bundlePath=mobile_bundle_name(scene.map_name); auto bundle=installation.read(bundlePath);
    if(bundle.size()<16) throw std::runtime_error("Invalid mobile bundle");
    auto existing=read_objects(bundle,16);std::set<std::string> ids;
    for(const auto& object:existing) ids.insert(object.id);
    // A project cannot silently replace a previously installed mobile object.
    for(const auto& object:scene.objects) if(object.type==2 || (object.type>=4 && object.type<=16) || (object.number("FLAGS")&0x2000))
        if(ids.count(object.id)) throw std::runtime_error("A project object is already installed. Use the original installation for this copy.");
    std::string name;
    if(!scene.location_names.empty()) {
        auto it=scene.location_names.find(language);if(it==scene.location_names.end()) it=scene.location_names.find("en");
        if(it==scene.location_names.end()) throw std::runtime_error("Add a location name in this language or an English fallback translation.");
        name=utf8_cp1252(it->second);
    }
    create_project(destination);
    struct PendingCopy {fs::path path;bool done=false;~PendingCopy(){if(!done){std::error_code ec;fs::remove_all(path,ec);}}} pending{destination};
    PlaytestResult result{destination,destination/"game",{},destination/"project"};
    export_building(scene,result.project);
    fs::create_directory(result.game);
    // Independent copies, including configuration and runtime overlays. Never
    // redirect a writable game path to the source installation.
    for(const auto& entry:fs::directory_iterator(source)) {
        auto lower=QString::fromStdString(entry.path().filename().string()).toLower();
        const QStringList folders={"data","modules","modern-ui","arcanummodern","art","proto","mes","rules","scr","dlg","maps","townmap","worldmap","sound","music","movies"};
        if(entry.is_directory() && folders.contains(lower)) {auto to=test_resource_path(result.game,entry.path().filename());fs::create_directories(to);copy_test_tree(entry.path(),to);}
        else if(entry.is_regular_file() && (lower.endsWith(".dat") || lower.endsWith(".cfg") || lower.endsWith(".ini") || lower.endsWith(".dll"))) fs::copy_file(entry.path(),test_resource_path(result.game,entry.path().filename()),fs::copy_options::overwrite_existing);
    }
    // A selected Windows engine may have its own FFmpeg/SDL runtime beside it.
    // Copy these after the installation libraries so that its matching versions win.
#ifdef Q_OS_WIN
    for(const auto& entry:fs::directory_iterator(fs::absolute(executable).parent_path()))
        if(entry.is_regular_file() && QString::fromStdString(entry.path().extension().string()).compare(".dll",Qt::CaseInsensitive)==0)
            fs::copy_file(entry.path(),test_resource_path(result.game,entry.path().filename()),fs::copy_options::overwrite_existing);
#endif
    result.executable=result.game/executable.filename();fs::copy_file(executable,result.executable,fs::copy_options::overwrite_existing);
    fs::permissions(result.executable,fs::status(executable).permissions());
    auto module=test_resource_path(result.game,"modules/Arcanum");
    auto save=test_resource_path(module,"Save");fs::create_directories(save);
    auto slot=saveIndex.stem().string();unsigned saved=0;
    for(const auto& entry:fs::directory_iterator(saveIndex.parent_path())) {
        auto filename=entry.path().filename().string(); auto ext=entry.path().extension().string();
        if(entry.is_regular_file() && ((entry.path().stem()==slot && (ext==".tfai" || ext==".tfaf" || ext==".bmp")) || (filename.rfind(slot,0)==0 && ext==".gsi"))) {fs::copy_file(entry.path(),save/entry.path().filename());saved++;}
    }
    if(saved<3) throw std::runtime_error("Missing save companions");
    for(const auto& entry:fs::recursive_directory_iterator(result.project/"exports")) if(entry.is_regular_file()) {
        auto target=test_resource_path(module,entry.path().lexically_relative(result.project/"exports"));fs::create_directories(target.parent_path());fs::copy_file(entry.path(),target,fs::copy_options::overwrite_existing);
        if(entry.path().extension()==".scr") {auto relative=entry.path().lexically_relative(result.project/"exports");auto early=test_resource_path(result.game/"data",relative);fs::create_directories(early.parent_path());fs::copy_file(entry.path(),early,fs::copy_options::overwrite_existing);}
        if(entry.path().extension()==".mob") {auto bytes=read_file(entry.path());bundle.insert(bundle.end(),bytes.begin(),bytes.end());}
    }
    write_bytes(test_resource_path(module,fs::u8path(bundlePath)),bundle);
    if(!name.empty()) {
        // Keep pre-existing editor locations and allocate a free resource key.
        auto merged=[&](const std::string& path) {return installation.contains(path) ? installation.read(path) : Bytes{};};
        auto locations=merged("rules/worlded_locations.mes"),names=merged("mes/worlded_location_names.mes");
        unsigned id=1;std::set<unsigned> used;for(const auto& m:read_messages(locations)) used.insert(m.id);for(const auto& m:read_messages(names)) used.insert(m.id);
        while(used.count(id)) ++id;
        auto append=[](Bytes& bytes,const std::string& value){bytes.insert(bytes.end(),value.begin(),value.end());};
        append(locations,"\n{"+std::to_string(id)+"}{"+std::to_string(scene.sector_id&0x3ffffff)+","+std::to_string(scene.sector_id>>26)+","+std::to_string(id)+"}\n");
        append(names,"\n{"+std::to_string(id)+"}{"+name+"}\n");
        write_bytes(test_resource_path(module,"rules/worlded_locations.mes"),locations);write_bytes(test_resource_path(module,"mes/worlded_location_names.mes"),names);
    }
    write_bytes(result.game/".worlded-playtest",Bytes{'1','\n'});
    // Native inventory generation is a creation-time operation in CE. Exported
    // MOBs bypass object_create, so materialize only empty project NPCs before
    // the player starts the copy; the authoring scene and source save stay intact.
    std::vector<std::string> inventoryRequests;
    for(const auto& object:scene.objects)if(object.type==16){
        unsigned generator=unsigned(object.number("CRITTER_INVENTORY_SOURCE"));
        if(!object.field("CRITTER_INVENTORY_SOURCE")){
            for(const auto& entry:installation.resources())if(entry.first.rfind("proto/",0)==0){auto name=entry.first.substr(6);unsigned id=0;auto parsed=std::from_chars(name.data(),name.data()+name.size(),id);if(parsed.ec==std::errc{} && id==object.prototype_id && fs::path(name).extension()==".pro"){auto prototypes=read_objects(installation.read(entry.first));if(!prototypes.empty())generator=unsigned(prototypes.front().number("CRITTER_INVENTORY_SOURCE"));break;}}
        }
        if(generator)inventoryRequests.push_back(object.id);
    }
    if(!inventoryRequests.empty()){
        auto helper=QDir(QCoreApplication::applicationDirPath()).filePath("worlded-native-inventory"
#ifdef Q_OS_WIN
            ".exe"
#endif
        );
        if(!QFileInfo(helper).isExecutable())throw std::runtime_error("The native WorldED inventory generator is missing (worlded-native-inventory). Build it with tools/worlded/build_engine_check.py --inventory.");
        auto generated=destination/"inventory-generated";fs::create_directory(generated);auto requests=destination/"inventory-requests.txt";std::string text;for(const auto& id:inventoryRequests)text+=id+"\n";write_bytes(requests,Bytes(text.begin(),text.end()));
        QProcess prepare;prepare.setProcessChannelMode(QProcess::MergedChannels);prepare.start(helper,{QString::fromStdString(fs::absolute(result.game).u8string()),QString::fromStdString(slot),QString::fromStdString(fs::absolute(requests).u8string()),QString::fromStdString(fs::absolute(generated).u8string()),QString::number(scene.sector_id&0x3ffffff),QString::number(scene.sector_id>>26)});
        if(!prepare.waitForStarted() || !prepare.waitForFinished(120000) || prepare.exitStatus()!=QProcess::NormalExit || prepare.exitCode()!=0){prepare.kill();prepare.waitForFinished();throw std::runtime_error("Inventory preparation failed: "+prepare.readAll().right(3000).toStdString());}
        auto log=prepare.readAll();write_bytes(destination/"inventory-generation.log",Bytes(log.begin(),log.end()));
        std::map<std::string,Bytes> replacements;for(const auto& file:fs::directory_iterator(generated))if(file.is_regular_file() && file.path().extension()==".mob"){auto data=read_file(file.path());auto objects=read_objects(data);if(objects.size()!=1)throw std::runtime_error("Invalid generated inventory object");replacements[objects.front().id]=std::move(data);}
        Bytes updated(bundle.begin(),bundle.begin()+16);for(const auto& object:read_objects(bundle,16))if(!replacements.count(object.id))updated.insert(updated.end(),bundle.begin()+object.offset,bundle.begin()+object.offset+object.size);
        for(const auto& entry:replacements){updated.insert(updated.end(),entry.second.begin(),entry.second.end());auto target=test_resource_path(module,fs::path("maps")/scene.map_name/("g_"+entry.first.substr(4)+".mob"));write_bytes(target,entry.second);}
        write_bytes(test_resource_path(module,fs::u8path(bundlePath)),updated);fs::remove_all(generated);fs::remove(requests);
    }

    QJsonObject launch{{"format","worlded-playtest"},{"executable",QString::fromStdString(executable.filename().u8string())},
        {"slot",QString::fromStdString(slot)},{"x",int(scene.sector_id&0x3ffffff)},{"y",int(scene.sector_id>>26)}};
    auto launchBytes=QJsonDocument(launch).toJson();write_bytes(destination/"playtest.json",Bytes(launchBytes.begin(),launchBytes.end()));
    // A self-contained launcher uses the same test arguments as the editor.
    // Quote the executable basename instead of interpreting user paths as code.
    auto quoted=[](std::string value){std::size_t pos=0;while((pos=value.find('\'',pos))!=std::string::npos){value.replace(pos,1,"'\\''");pos+=4;}return "'"+value+"'";};
#ifndef Q_OS_WIN
    std::string script="#!/bin/sh\nset -eu\ncd -- \"$(CDPATH= cd -- \"$(dirname -- \"$0\")\" && pwd)/game\"\n";
#ifdef Q_OS_LINUX
    script+="export SDL_VIDEO_DRIVER=x11 SDL_RENDER_DRIVER=software SDL_FRAMEBUFFER_ACCELERATION=0\n";
#endif
    script+="exec "+quoted("./"+executable.filename().u8string())+" --worlded-test "+slot+" "+std::to_string(scene.sector_id&0x3ffffff)+" "+std::to_string(scene.sector_id>>26)+" > ../test-launch.log 2>&1\n";
#ifdef Q_OS_MACOS
    auto launcher=destination/"Launch test.command";
#else
    auto launcher=destination/"Launch test.sh";
#endif
    write_bytes(launcher,Bytes(script.begin(),script.end()));fs::permissions(launcher,fs::perms::owner_exec|fs::perms::group_exec|fs::perms::others_exec,fs::perm_options::add);
#else
    (void)quoted;
#endif
    pending.done=true;
    return result;
}
}
