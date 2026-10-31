#ifndef ARCANUM_MODERN_WORLD_UPDATES_INTERNAL_H
#define ARCANUM_MODERN_WORLD_UPDATES_INTERNAL_H
#include "game/modern_world_updates.h"
/* Core loader/codec owned by parent. No mutation on validation failure. */
extern RwPackage* rw_packages;
extern unsigned rw_package_count;
extern bool rw_active,rw_applying;
void rw_error(const char* format,...);
void rw_log(const char* package,const char* operation,const char* outcome,const char* detail);
void rw_sha256(const void* bytes,size_t size,uint8_t digest[32]);
bool rw_hash_stream(TigFile* stream,uint8_t digest[32]);
bool rw_operation_validate(const RwOperation* operation,char* error,size_t size);
bool rw_hash_resource(const char* path,uint8_t digest[32]);
bool rw_package_read(const char* directory,RwPackage* out);
void rw_package_free(RwPackage* package);
/* Ledger + backup owned by state module. Return -1 for digest mismatch. */
void rw_state_reset(void);
void rw_state_set_directory(const char* directory);
bool rw_save_recover(void);
bool rw_save_stage(char stage[9]);
bool rw_save_publish(const char* stage,const char* slot,const char* description);
void rw_save_discard(const char* stage);
bool rw_state_load(const char* slot);
bool rw_state_save(void);
bool rw_state_validate_packages(void);
int rw_state_outcome(const RwPackage* package,const RwOperation* operation);
bool rw_state_record(const RwPackage* package,const RwOperation* operation,unsigned outcome,const char* detail);
bool rw_state_backup(const char* slot);
bool rw_state_roof_get(const char* map,uint64_t sector,unsigned index,uint32_t* value);
bool rw_state_roof_put(const char* map,uint64_t sector,unsigned index,uint32_t value);
/* Native operation implementation owned by operations module. */
void rw_operations_reset(void);
bool rw_operations_healthy(void);
typedef bool (*RwPackagePrepare)(const RwPackage* package,Sector* sector,void* context);
typedef void (*RwAddObserver)(const RwPackage* package,const RwOperation* operation,int64_t object,bool created,void* context);
void rw_operations_set_observers(RwPackagePrepare prepare,RwAddObserver added,void* context);
void rw_scene_hide(int64_t object);
void rw_scene_ground(Sector* sector,unsigned index,uint32_t value);
void rw_scene_roof(Sector* sector,unsigned index,uint32_t value);
void rw_scene_block(Sector* sector,unsigned index,bool blocked);
void rw_operations_source(int64_t id,const char* source);
void rw_operations_source_digest(int64_t id,const char* source,const uint8_t digest[32]);
void rw_operations_sector(int64_t id,Sector* sector);
void rw_operations_capture(Sector* sector);
bool rw_operations_global(void);
bool rw_operations_finish_load(void);
#endif
