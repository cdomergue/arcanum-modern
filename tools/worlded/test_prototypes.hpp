#pragma once
#include "src/prototype_catalog.hpp"
#include "src/workspace.hpp"

inline void checkDefaultPrototypes(const std::filesystem::path& fixture) {
    using namespace worlded;
    namespace fs=std::filesystem;
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    Installation source(fixture);
    auto bytes=source.read("proto/001020.pro");
    QTemporaryDir temporary;
    auto root=fs::u8path(temporary.path().toUtf8().constData());
    write_bytes(root/"game/data/mes/fixture.mes",bytes);
    for(auto file : {"npc","monster","unique"}) write_bytes(root/"game/data/rules"/(std::string(file)+".txt"),{});
    write_bytes(root/"output/001020.pro",bytes);
    Installation fresh(root/"game");
    require(fresh.add_default_prototypes(root/"output")==1,"Fresh installation did not accept generated defaults");
    fs::remove_all(root/"output");
    require(fresh.read("proto/001020.pro")==bytes,"Generated resources did not survive temporary directory removal");
    write_bytes(root/"output/001020.pro",bytes);
    write_bytes(root/"game/data/proto/001020 - Custom.pro",bytes);
    Installation custom(root/"game");
    require(custom.add_default_prototypes(root/"output")==0,"Generated default overrode custom prototype filename");
    require(!custom.contains("proto/001020.pro") && custom.read("proto/001020 - Custom.pro")==bytes,"Custom prototype identity lost");
    write_bytes(root/"output/009999.pro",{1,2,3});
    Installation invalid(root/"game");
    bool rejected=false;try{invalid.add_default_prototypes(root/"output");}catch(...){rejected=true;}
    require(rejected && !invalid.contains("proto/001020.pro"),"Invalid generated catalog was partially published");
    fs::remove_all(root/"game/data/proto");
    Installation generated(root/"game");
    prepare_default_prototypes(generated,QCoreApplication::applicationFilePath());
    require(generated.diagnostics.empty() && generated.read("proto/001020.pro")==bytes,"Native generation subprocess contract failed");
    require(!fs::exists(root/"game/proto") && !fs::exists(root/"game/data/proto"),"Library generation wrote to the source installation");
    Installation absent(root/"game");
    prepare_default_prototypes(absent,temporary.filePath("missing-helper"));
    require(!absent.diagnostics.empty() && !absent.contains("proto/001020.pro"),"Missing helper did not report a diagnostic");
    auto log=temporary.filePath("worlded-catalog.log");
    QFile oversized(log);require(oversized.open(QIODevice::WriteOnly),"Cannot stage diagnostic rotation");
    oversized.write(QByteArray(257*1024,'x'));oversized.close();log_prototype_catalog("rotation-test=ok",temporary.path());
    require(QFileInfo(log).size()<1024 && QFileInfo(log+".1").size()==257*1024,"Catalog diagnostics did not rotate");
}
