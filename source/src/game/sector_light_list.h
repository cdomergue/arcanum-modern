#ifndef ARCANUM_GAME_SECTOR_LIST_LIST_H_
#define ARCANUM_GAME_SECTOR_LIST_LIST_H_

#include <tig/tig.h>

#include "game/light.h"
#include "game/sector_block_list.h"
#include "game/sector_tile_list.h"

typedef struct SectorLightList {
    SectorBlockListNode* head;
    bool modified;
} SectorLightList;

bool sector_light_list_init(SectorLightList* list);
bool sector_light_list_reset(SectorLightList* list);
bool sector_light_list_exit(SectorLightList* list);
bool sector_light_list_add(SectorLightList* list, Light* light);
bool sector_light_list_remove(SectorLightList* list, Light* light);
bool sector_light_list_update(SectorLightList* list, Light* light, int64_t loc, int offset_x, int offset_y);
bool sector_light_list_load(SectorLightList* list, TigFile* stream, int64_t sector_id);
bool sector_light_list_load_with_dif(SectorLightList* list, TigFile* sec_stream, TigFile* dif_stream, int64_t sector_id);
bool sector_light_list_save(SectorLightList* list, TigFile* stream);
bool sector_light_list_is_modified(SectorLightList* list);
bool sector_light_list_save_with_dif(SectorLightList* list, TigFile* stream);
void sector_light_list_fold(SectorLightList* list, int64_t sector_id, SectorTileList* tiles);

#endif /* ARCANUM_GAME_SECTOR_LIST_LIST_H_ */
