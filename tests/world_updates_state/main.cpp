#include <filesystem>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#include <cstdarg>
#include <cstring>
#include <chrono>
extern "C" {
#include "game/modern_world_updates_internal.h"
RwPackage* rw_packages=nullptr;unsigned rw_package_count=0;bool rw_active=false,rw_applying=false;int fail_copy=0;
static char last_error[512];
void rw_error(const char* format,...){va_list args;va_start(args,format);vsnprintf(last_error,sizeof(last_error),format,args);va_end(args);}
// This harness exercises ledger checksum use; cryptographic SHA-256 is supplied by the native core.
void rw_sha256(const void* raw,size_t n,uint8_t digest[32]){const auto bytes=static_cast<const uint8_t*>(raw);uint64_t hash=1469598103934665603ull;for(size_t i=0;i<n;i++)hash=(hash^bytes[i])*1099511628211ull;for(int i=0;i<32;i++)digest[i]=uint8_t(hash>>(8*(i%8)));}
bool rw_package_read(const char*,RwPackage*){return false;}void rw_package_free(RwPackage*){}
bool tig_file_exists(const char* p,void*){return std::filesystem::exists(p);}bool tig_file_is_directory(const char* p){return std::filesystem::is_directory(p);}
bool tig_file_mkdir(const char* p){std::error_code ec;return std::filesystem::create_directory(p,ec)||std::filesystem::is_directory(p);}bool tig_file_rmdir(const char* p){std::error_code ec;return std::filesystem::remove(p,ec);}
TigFile* tig_file_fopen(const char* p,const char* mode){if(!strcmp(mode,"wb")&&!strstr(p,"publish.tmp")&&fail_copy>0&&!--fail_copy)return nullptr;return fopen(p,mode);}
int tig_file_remove(const char* p){std::error_code ec;return std::filesystem::remove(p,ec)?0:1;}int tig_file_rename(const char* a,const char* b){std::error_code ec;std::filesystem::rename(a,b,ec);return ec?1:0;}
void tig_file_list_create(TigFileList* list,const char* pattern){std::filesystem::path p(pattern);auto prefix=p.filename().string();prefix.resize(prefix.find('*'));std::vector<std::string> names;std::error_code ec;if(std::filesystem::exists(p.parent_path()))for(auto& e:std::filesystem::directory_iterator(p.parent_path(),ec))if(e.is_regular_file()&&e.path().filename().string().rfind(prefix,0)==0)names.push_back(e.path().filename().string());list->count=unsigned(names.size());list->entries=static_cast<TigFileEntry*>(calloc(names.size()?names.size():1,sizeof(TigFileEntry)));for(size_t i=0;i<names.size();i++)strcpy(list->entries[i].path,("Arcanum\\Save\\"+names[i]).c_str());}
void tig_file_list_destroy(TigFileList* list){free(list->entries);list->entries=nullptr;list->count=0;}
int tig_file_filelength(TigFile* f){auto pos=ftell(f);fseek(f,0,SEEK_END);auto n=ftell(f);fseek(f,pos,SEEK_SET);return int(n);}
}
namespace fs=std::filesystem;
static void check(bool condition,const char* why){if(!condition)throw std::runtime_error(std::string(why)+": "+last_error);}
static void write(const fs::path& p,const std::string& bytes){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<bytes;check(bool(f),"fixture write failed");}
static std::string read(const fs::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
int main(){auto root=fs::temp_directory_path()/("world-update-state-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(root/"Save/Current");try{
    rw_state_set_directory(root.string().c_str());RwOperation operations[2]{};strcpy(operations[0].id,"add-decor");memset(operations[0].digest,1,32);strcpy(operations[1].id,"move-chest");memset(operations[1].digest,2,32);RwResource resource{};strcpy(resource.path,"objects/decor.mob");memset(resource.digest,3,32);RwPackage package{};strcpy(package.id,"test.world");strcpy(package.module,"Arcanum");package.revision=1;memset(package.digest,4,32);package.operation_count=1;package.operations=operations;package.resource_count=1;package.resources=&resource;rw_packages=&package;rw_package_count=1;
    check(rw_state_load("Slot0001"),"old save without ledger rejected");check(rw_state_validate_packages(),"first package validation failed");check(rw_state_record(&package,&operations[0],RW_APPLIED,"fixture"),"record failed");check(rw_state_roof_put("arcanum1",123,7,0xA0000000),"roof put failed");check(rw_state_save(),"save ledger failed");check(rw_state_load("Slot0001")&&rw_state_validate_packages(),"reopen ledger failed");check(rw_state_outcome(&package,&operations[0])==RW_APPLIED,"completed add lost");uint32_t roof=0;check(rw_state_roof_get("arcanum1",123,7,&roof)&&roof==0xA0000000,"roof cell lost");
    package.revision=3;package.operation_count=2;memset(package.digest,5,32);check(rw_state_validate_packages(),"skipped cumulative revision rejected");check(rw_state_outcome(&package,&operations[1])==RW_PENDING,"new operation unexpectedly completed");operations[0].digest[0]^=1;check(!rw_state_validate_packages(),"changed published operation accepted");operations[0].digest[0]^=1;package.revision=1;check(!rw_state_validate_packages(),"package downgrade accepted");package.revision=3;resource.digest[0]^=1;check(!rw_state_validate_packages(),"changed published resource accepted");resource.digest[0]^=1;check(rw_state_save(),"save upgraded ledger failed");rw_package_count=0;check(!rw_state_validate_packages(),"missing required package accepted");rw_package_count=1;
    auto ledger=root/"Save/Current/renaissance-world.sav";auto bytes=read(ledger);bytes[12]^=1;write(ledger,bytes);check(!rw_state_load("Slot0001"),"corrupt ledger accepted");
    // Publication succeeds, overwrites only this slot and removes old GSI names.
    write(root/"Save/Slot0001.tfaf","old-data");write(root/"Save/Slot0001.tfai","old-index");write(root/"Save/Slot0001.bmp","old-picture");write(root/"Save/Slot0001Old name.gsi","old-meta");write(root/"Save/Slot0002.tfaf","untouched");char stage[9];check(rw_save_stage(stage),"stage allocation failed");for(auto ext:{".tfaf",".tfai",".bmp","New name.gsi"})write(root/"Save"/(std::string(stage)+ext),std::string("new-")+ext);check(rw_save_publish(stage,"Slot0001","New name"),"publication failed");check(read(root/"Save/Slot0001.tfaf")=="new-.tfaf"&&!fs::exists(root/"Save/Slot0001Old name.gsi")&&read(root/"Save/Slot0002.tfaf")=="untouched","publication changed wrong files");
    // Four backups and the data archive copy succeed; the index copy then fails.
    check(rw_save_stage(stage),"second stage failed");for(auto ext:{".tfaf",".tfai",".bmp","Broken.gsi"})write(root/"Save"/(std::string(stage)+ext),"bad-replacement");fail_copy=6;check(!rw_save_publish(stage,"Slot0001","Broken"),"injected copy failure ignored");check(read(root/"Save/Slot0001.tfaf")=="new-.tfaf"&&read(root/"Save/Slot0001New name.gsi")=="new-New name.gsi"&&!fs::exists(root/"Save/Slot0001Broken.gsi"),"previous save was not restored");check(rw_save_recover(),"recovery not idempotent");
    // Reconstruct an interrupted publication from disk, with no in-memory transaction.
    auto recovery=root/"Save/WorldUpdates/crash-fixture";fs::create_directories(recovery);
    std::vector<std::string> oldNames{"Slot0001.tfaf","Slot0001.tfai","Slot0001.bmp","Slot0001New name.gsi"};for(auto& name:oldNames)fs::copy_file(root/"Save"/name,recovery/name);
    std::string journal("RWTRANS",8);auto word=[&](unsigned value){for(int i=0;i<4;i++)journal.push_back(char(value>>(8*i)));};auto string=[&](const std::string& value){word(unsigned(value.size()));journal+=value;};string("crash-fixture");string("Slot0001");string("Slot0001Interrupted.gsi");word(unsigned(oldNames.size()));for(auto& name:oldNames)string(name);write(root/"Save/WorldUpdates/publish.pending",journal);
    write(root/"Save/Slot0001.tfaf","interrupted-data");fs::remove(root/"Save/Slot0001New name.gsi");rw_state_reset();check(rw_save_recover(),"disk-only recovery failed");check(read(root/"Save/Slot0001.tfaf")=="new-.tfaf"&&read(root/"Save/Slot0001New name.gsi")=="new-New name.gsi","restart recovery lost previous slot");
    // A conflict-only ledger is retried, so it needs a fresh loaded-slot backup.
    fs::remove(ledger);check(rw_state_load("Slot0001")&&rw_state_validate_packages(),"conflict fixture ledger failed");
    check(rw_state_record(&package,&operations[0],RW_APPLIED,"fixture")&&rw_state_record(&package,&operations[1],RW_CONFLICT,"guard differs")&&rw_state_save(),"conflict fixture save failed");
    check(rw_state_load("Slot0001")&&rw_state_validate_packages(),"conflict fixture reload failed");
    auto packageDir=root/"package";strcpy(package.directory,packageDir.string().c_str());write(packageDir/"runtime.rwp","fixture runtime");write(packageDir/resource.path,"fixture resource");
    fail_copy=1;check(!rw_state_backup("Slot0001"),"conflict retry bypassed mandatory backup failure");check(rw_state_backup("Slot0001"),"conflict retry backup failed");
    bool conflictBackup=false;for(auto& entry:fs::directory_iterator(root/"Save/WorldUpdates"))if(entry.is_directory()&&entry.path().filename().string().rfind("before-Slot0001-",0)==0&&fs::exists(entry.path()/"README.txt"))conflictBackup=read(entry.path()/"Slot0001.tfaf")=="new-.tfaf";
    check(conflictBackup,"conflict retry did not preserve actual loaded slot");
    rw_state_reset();check(fs::weakly_canonical(root).parent_path()==fs::canonical(fs::temp_directory_path()),"unsafe fixture cleanup path");fs::remove_all(root);puts("PASS: ledger persistence, immutability, skipped revisions, roof state, checksum, save publication and fault recovery");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\nFixture retained at %s\n",e.what(),root.string().c_str());return 1;}}
