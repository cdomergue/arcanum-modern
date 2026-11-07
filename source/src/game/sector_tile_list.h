#ifndef ARCANUM_GAME_SECTOR_TILE_LIST_H_
#define ARCANUM_GAME_SECTOR_TILE_LIST_H_

#include <tig/tig.h>

typedef struct SectorTileList {
    /* 0000 */ tig_art_id_t art_ids[4096];
    /* 4000 */ uint32_t difmask[128];
    /* 4200 */ uint8_t dif;
} SectorTileList;

bool sector_tile_list_init(SectorTileList* list, tig_art_id_t* art_id_ptr);
bool sector_tile_list_reset(SectorTileList* list);
bool sector_tile_list_exit(SectorTileList* list);
bool sector_tile_list_load(SectorTileList* list, TigFile* stream);
bool sector_tile_list_save(SectorTileList* list, TigFile* stream);
bool sector_tile_list_load_with_dif(SectorTileList* list, TigFile* stream);
bool sector_tile_list_save_with_dif(SectorTileList* list, TigFile* stream);
void sector_list_tile_precache_art(SectorTileList* list);

#endif /* ARCANUM_GAME_SECTOR_TILE_LIST_H_ */
