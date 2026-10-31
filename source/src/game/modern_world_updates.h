#ifndef ARCANUM_MODERN_WORLD_UPDATES_H
#define ARCANUM_MODERN_WORLD_UPDATES_H
#include <stdbool.h>
#include <stdint.h>
#include "../../../shared/world_updates_format.h"
#include "game/sector.h"
/* Engine lifecycle: no bytes are added to the version-25 module stream. */
bool modern_world_updates_resources_init(void);
void modern_world_updates_tick(void);
bool modern_world_updates_module_load(const char* module);
void modern_world_updates_reset(void);
void modern_world_updates_new_game(void);
void modern_world_updates_abort_load(void);
bool modern_world_updates_load(const char* slot);
bool modern_world_updates_finish_load(void);
bool modern_world_updates_save(void);
void modern_world_updates_sector_source(int64_t id,const char* source);
void modern_world_updates_sector_source_digest(int64_t id,const char* source,const uint8_t digest[32]);
void modern_world_updates_sector_ready(int64_t id,Sector* sector);
void modern_world_updates_capture_sector(Sector* sector);
const char* modern_world_updates_error(void);
#endif
