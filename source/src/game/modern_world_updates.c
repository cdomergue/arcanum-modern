#include "game/modern_world_updates_internal.h"
#include "../../../shared/world_updates_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include "game/gamelib.h"
#include "game/map.h"
#include "game/player.h"
#include "game/mes.h"
#include <tig/tig.h>
RwPackage* rw_packages;
unsigned rw_package_count;
bool rw_active,rw_applying;
static RwPackage* catalog;
static unsigned catalog_count;
static bool initialized;
static bool awaiting_new_game;
static char last_error[512];
void rw_error(const char* format,...){va_list args;va_start(args,format);vsnprintf(last_error,sizeof(last_error),format,args);va_end(args);tig_debug_printf("Renaissance world updates: %s\n",last_error);FILE* file=fopen("ArcanumModern/world-updates/errors.log","ab");if(file){fprintf(file,"%s\n",last_error);fclose(file);}}
const char* modern_world_updates_error(void){return last_error;}
bool rw_hash_stream(TigFile* stream,uint8_t digest[32]){int position=tig_file_ftell(stream),size=tig_file_filelength(stream);if(position<0 || size<0 || (unsigned)size>RW_MAX_PACKAGE_BYTES)return false;uint8_t* data=(uint8_t*)malloc(size?size:1);if(!data)return false;bool ok=tig_file_fseek(stream,0,SEEK_SET)==0 && tig_file_fread(data,1,size,stream)==(size_t)size;if(tig_file_fseek(stream,position,SEEK_SET)!=0)ok=false;if(ok)rw_sha256(data,size,digest);free(data);return ok;}
bool rw_hash_resource(const char* path,uint8_t digest[32]){TigFile* stream=tig_file_fopen(path,"rb");if(!stream)return false;bool ok=rw_hash_stream(stream,digest);if(tig_file_fclose(stream)!=0)ok=false;return ok;}
bool rw_package_read(const char* directory,RwPackage* out){char error[256];if(!rw_codec_read(directory,out,error,sizeof(error))){rw_error("%s: %s",directory,error);return false;}for(unsigned i=0;i<out->operation_count;i++)if(!rw_operation_validate(&out->operations[i],error,sizeof(error))){rw_error("%s/%s: %s",out->id,out->operations[i].id,error);rw_codec_free(out);return false;}return true;}
void rw_package_free(RwPackage* package){rw_codec_free(package);}
static int find_package(const char* id){for(unsigned i=0;i<catalog_count;i++)if(!strcmp(catalog[i].id,id))return (int)i;return -1;}
static bool ordered[RW_MAX_PACKAGES][RW_MAX_PACKAGES];
static bool overlap(const RwOperation* a,const RwOperation* b){
    if(a->kind==RW_GLOBAL || b->kind==RW_GLOBAL)return a->kind==b->kind && a->index==b->index && !strcmp(a->field,b->field);
    if(strcmp(a->map,b->map) || a->sector!=b->sector)return false;
    if(a->kind>=RW_GROUND && a->kind<=RW_BLOCK)return a->kind==b->kind && a->index==b->index;
    if(b->kind>=RW_GROUND && b->kind<=RW_BLOCK)return false;
    bool same=a->guid[0] && b->guid[0]?!strcmp(a->guid,b->guid):a->selector==RW_STATIC && b->selector==RW_STATIC && a->ordinal==b->ordinal && !memcmp(a->base_hash,b->base_hash,32);
    if(!same)return false;if(a->kind==RW_ADD || b->kind==RW_ADD)return true;
    if(a->kind==RW_FIELD && b->kind==RW_FIELD)return !strcmp(a->field,b->field);
    if((a->kind==RW_FLAGS || a->kind==RW_HIDE) && (b->kind==RW_FLAGS || b->kind==RW_HIDE))return ((a->kind==RW_HIDE?2:a->mask)&(b->kind==RW_HIDE?2:b->mask))!=0;
    return a->kind==RW_MOVE && b->kind==RW_MOVE;
}
static bool validate_catalog(void){
    memset(ordered,0,sizeof(ordered));for(unsigned i=0;i<catalog_count;i++)for(unsigned d=0;d<catalog[i].dependency_count;d++){int j=find_package(catalog[i].dependencies[d].id);if(j<0 || catalog[j].revision<catalog[i].dependencies[d].revision || SDL_strcasecmp(catalog[i].module,catalog[j].module)){rw_error("Package %s needs compatible dependency %s revision %u",catalog[i].id,catalog[i].dependencies[d].id,catalog[i].dependencies[d].revision);return false;}ordered[j][i]=true;}
    for(unsigned k=0;k<catalog_count;k++)for(unsigned i=0;i<catalog_count;i++)for(unsigned j=0;j<catalog_count;j++)ordered[i][j]|=ordered[i][k]&&ordered[k][j];
    for(unsigned i=0;i<catalog_count;i++){if(ordered[i][i]){rw_error("Dependency cycle at %s",catalog[i].id);return false;}for(unsigned j=0;j<i;j++)if(!SDL_strcasecmp(catalog[i].module,catalog[j].module)){
        for(unsigned a=0;a<catalog[i].operation_count;a++)for(unsigned b=0;b<catalog[j].operation_count;b++)if(overlap(&catalog[i].operations[a],&catalog[j].operations[b]) && (!ordered[i][j]&&!ordered[j][i])){rw_error("Unordered overlapping operations %s/%s and %s/%s",catalog[i].id,catalog[i].operations[a].id,catalog[j].id,catalog[j].operations[b].id);return false;}
        for(unsigned a=0;a<catalog[i].resource_count;a++)for(unsigned b=0;b<catalog[j].resource_count;b++)if(strncmp(catalog[i].resources[a].path,"objects/",8) && !strcmp(catalog[i].resources[a].path,catalog[j].resources[b].path) && memcmp(catalog[i].resources[a].digest,catalog[j].resources[b].digest,32)){rw_error("Conflicting package resource %s",catalog[i].resources[a].path);return false;}
    }}
    RwPackage* sorted=(RwPackage*)calloc(catalog_count?catalog_count:1,sizeof(*sorted));if(!sorted){rw_error("Cannot sort packages");return false;}bool done[RW_MAX_PACKAGES]={false};for(unsigned n=0;n<catalog_count;n++){int selected=-1;for(unsigned i=0;i<catalog_count;i++)if(!done[i]){bool ready=true;for(unsigned j=0;j<catalog_count;j++)if(ordered[j][i]&&!done[j])ready=false;if(ready){selected=(int)i;break;}}if(selected<0){free(sorted);rw_error("Cannot order dependencies");return false;}done[selected]=true;sorted[n]=catalog[selected];}free(catalog);catalog=sorted;return true;
}
/* Engine script/dialog/prototype lookup uses numeric prefixes, not whole filenames. */
static unsigned resource_number(const char* path) {
    const char* name=strrchr(path,'/'); if(!name)name=strrchr(path,'\\');
    name=name?name+1:path; if(*name<'0'||*name>'9')return 0;
    return (unsigned)strtoul(name,NULL,10);
}
static bool resource_compatible(const RwPackage* package) {
    for(unsigned i=0;i<package->resource_count;i++) {
        const RwResource* r=&package->resources[i];
        if(!strncmp(r->path,"objects/",8))continue;
        TigFileInfo info;
        if(tig_file_exists(r->path,&info)) {
            uint8_t digest[32];
            if(!rw_hash_resource(r->path,digest)||memcmp(digest,r->digest,32)) {
                rw_error("Package %s would replace existing resource %s; ABI 1 permits new resources only",package->id,r->path);return false;
            }
        }
        unsigned number=resource_number(r->path);
        if(!number){rw_error("Resource needs a nonzero numeric ID: %s",r->path);return false;}
        const char* slash=strchr(r->path,'/');char pattern[64];
        snprintf(pattern,sizeof(pattern),"%.*s/*",(int)(slash-r->path),r->path);
        TigFileList files;tig_file_list_create(&files,pattern);
        bool compatible=true;
        for(unsigned j=0;j<files.count;j++) {
            const char* name=files.entries[j].path;
            for(const char* at=name;*at;at++)if(*at=='/'||*at=='\\')name=at+1;
            if(resource_number(name)==number && SDL_strcasecmp(name,slash+1)) {
                rw_error("Resource ID collision: %s and %s",r->path,files.entries[j].path);compatible=false;break;
            }
        }
        tig_file_list_destroy(&files);if(!compatible)return false;
    }
    /* Repositories are mounted before module selection, so IDs must be unique across modules too. */
    for(unsigned i=0;i<package->resource_count;i++) {
        const RwResource* a=&package->resources[i];if(!strncmp(a->path,"objects/",8))continue;
        const char* slash=strchr(a->path,'/');size_t prefix=(size_t)(slash-a->path)+1;
        for(unsigned p=0;p<catalog_count;p++)for(unsigned j=0;j<catalog[p].resource_count;j++) {
            const RwResource* b=&catalog[p].resources[j];
            if(!strncmp(a->path,b->path,prefix) && resource_number(a->path)==resource_number(b->path) &&
                (strcmp(a->path,b->path)||memcmp(a->digest,b->digest,32))) {
                rw_error("Packages collide on resource ID: %s and %s",a->path,b->path);return false;
            }
        }
    }
    return true;
}
static bool load_registry(const char* path, bool fallback)
{
    FILE* probe = fopen(path, "rb");
    if (!probe) {
        if (errno == ENOENT) return true;
        rw_error("Cannot read world update registry %s", path); return false;
    }
    fclose(probe);
    mes_file_handle_t index;
    if (!mes_load(path, &index)) { rw_error("Invalid world update registry %s", path); return false; }
    MesFileEntry entry;
    bool ok = true;
    for (bool more = mes_find_first(index, &entry); more; more = mes_find_next(index, &entry)) {
        if (!rw_codec_path(entry.str)) { rw_error("Invalid package registry path"); ok = false; break; }
        char folder[RW_PATH_SIZE];
        RwPackage incoming;
        if (snprintf(folder, sizeof(folder), "ArcanumModern/world-updates/%s", entry.str) >= (int)sizeof(folder)
            || !rw_package_read(folder, &incoming)) { ok = false; break; }
        char expected[RW_PATH_SIZE];
        snprintf(expected, sizeof(expected), "%s/%u", incoming.id, incoming.revision);
        if (strcmp(expected, entry.str)) {
            rw_error("Registry path does not match package identity: %s", entry.str);
            rw_codec_free(&incoming); ok = false; break;
        }
        int previous = find_package(incoming.id);
        if (previous >= 0) {
            /* A manually installed revision pins that package over the bundled default. */
            if (!fallback || (catalog[previous].revision == incoming.revision
                    && memcmp(catalog[previous].digest, incoming.digest, 32))) {
                rw_error("Duplicate or altered installed package %s", incoming.id); ok = false;
            }
            rw_codec_free(&incoming);
            if (!ok) break;
            continue;
        }
        if (catalog_count >= RW_MAX_PACKAGES) {
            rw_error("Too many installed world update packages"); rw_codec_free(&incoming); ok = false; break;
        }
        catalog[catalog_count++] = incoming;
    }
    mes_unload(index);
    return ok;
}
bool modern_world_updates_resources_init(void)
{
    if (initialized) return true;
    last_error[0] = 0;
    catalog = (RwPackage*)calloc(RW_MAX_PACKAGES, sizeof(*catalog));
    if (!catalog) { rw_error("Out of memory reading update registry"); return false; }
    bool ok = load_registry("ArcanumModern/world-updates/index.mes", false)
        && load_registry("ArcanumModern/world-updates/bundled.mes", true);
    if (ok) ok = validate_catalog();
    if (ok) for (unsigned i = 0; i < catalog_count; i++) {
        if (!resource_compatible(&catalog[i])) { ok = false; break; }
    }
    unsigned mounted = 0;
    if (ok) for (; mounted < catalog_count; mounted++) {
        if (!tig_file_repository_add(catalog[mounted].directory)) {
            rw_error("Cannot mount package %s", catalog[mounted].id); ok = false; break;
        }
    }
    if (!ok) {
        for (unsigned i = 0; i < mounted; i++) tig_file_repository_remove(catalog[i].directory);
        for (unsigned i = 0; i < catalog_count; i++) rw_codec_free(&catalog[i]);
        free(catalog); catalog = NULL; catalog_count = 0;
        return false;
    }
    initialized = true;
    return true;
}
void modern_world_updates_reset(void){awaiting_new_game=false;rw_active=false;rw_applying=false;last_error[0]=0;rw_state_reset();rw_operations_reset();}
void modern_world_updates_new_game(void){modern_world_updates_reset();awaiting_new_game=true;}
void modern_world_updates_abort_load(void){awaiting_new_game=false;rw_active=false;rw_applying=false;rw_state_reset();rw_operations_reset();if(!last_error[0])rw_error("Save loading failed; world updates remain disabled until a new game or successful load");}
bool modern_world_updates_module_load(const char* module){modern_world_updates_reset();free(rw_packages);rw_packages=NULL;rw_package_count=0;if(!modern_world_updates_resources_init())return false;const char* name=module;for(const char* p=module;*p;p++)if(*p=='/'||*p=='\\')name=p+1;rw_packages=(RwPackage*)calloc(catalog_count?catalog_count:1,sizeof(*rw_packages));if(!rw_packages){rw_error("Out of memory selecting update packages");return false;}for(unsigned i=0;i<catalog_count;i++)if(!SDL_strcasecmp(catalog[i].module,name)){if(!resource_compatible(&catalog[i]))return false;rw_packages[rw_package_count++]=catalog[i];}awaiting_new_game=true;return true;}
bool modern_world_updates_load(const char* slot){modern_world_updates_reset();if(!rw_state_load(slot) || !rw_state_validate_packages())return false;if(rw_package_count && !rw_state_backup(slot))return false;return true;}
bool modern_world_updates_finish_load(void){awaiting_new_game=false;if(!rw_state_validate_packages())return false;rw_active=true;if(!rw_operations_global() || !rw_operations_finish_load()){rw_active=false;return false;}return true;}
bool modern_world_updates_save(void){if(last_error[0] || (rw_package_count && !rw_active) || !rw_operations_healthy() || !rw_state_validate_packages())return false;return rw_state_save();}
void modern_world_updates_sector_source(int64_t id,const char* source){if(rw_package_count)rw_operations_source(id,source);}
void modern_world_updates_sector_source_digest(int64_t id,const char* source,const uint8_t digest[32]){if(rw_package_count)rw_operations_source_digest(id,source,digest);}
void modern_world_updates_sector_ready(int64_t id,Sector* sector){if(rw_package_count)rw_operations_sector(id,sector);}
void modern_world_updates_capture_sector(Sector* sector){if(rw_package_count)rw_operations_capture(sector);}
void modern_world_updates_tick(void){if(awaiting_new_game && !rw_active && rw_package_count && !last_error[0] && !gamelib_in_load() && !map_is_clearing_objects() && player_get_local_pc_obj())modern_world_updates_finish_load();}
