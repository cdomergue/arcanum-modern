#ifndef ARCANUM_GAME_SECTOR_ROOF_LIST_H_
#define ARCANUM_GAME_SECTOR_ROOF_LIST_H_

#include <tig/tig.h>

#define SECTOR_ROOF_LIST_SIZE 256

typedef struct SectorRoofList {
    int empty;
    tig_art_id_t art_ids[SECTOR_ROOF_LIST_SIZE];
} SectorRoofList;

bool sector_roof_list_init(SectorRoofList* list);
bool sector_roof_list_reset(SectorRoofList* list);
bool sector_roof_list_exit(SectorRoofList* list);
bool sector_roof_list_load(SectorRoofList* list, TigFile* stream);
bool sector_roof_list_save(SectorRoofList* list, TigFile* stream);
bool sector_roof_list_is_modified(SectorRoofList* list);
bool sector_roof_list_load_with_dif(SectorRoofList* list, TigFile* stream);
bool sector_roof_list_save_with_dif(SectorRoofList* list, TigFile* stream);
void sector_roof_list_precache_art(SectorRoofList* list);

#endif /* ARCANUM_GAME_SECTOR_ROOF_LIST_H_ */
