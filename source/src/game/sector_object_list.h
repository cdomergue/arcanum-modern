#ifndef ARCANUM_GAME_SECTOR_OBJECT_LIST_H_
#define ARCANUM_GAME_SECTOR_OBJECT_LIST_H_

#include "game/object_node.h"

typedef struct SectorObjectList {
    /* 0000 */ ObjectNode* heads[4096];
    /* 4000 */ int modified;
    /* 4004 */ int next_temp_id;
} SectorObjectList;

bool sector_object_list_init(SectorObjectList* list);
bool sector_object_list_reset(SectorObjectList* list);
bool sector_object_list_exit(SectorObjectList* list);
bool objlist_insert(SectorObjectList* list, int64_t obj);
bool objlist_remove(SectorObjectList* list, int64_t obj);
bool objlist_move(SectorObjectList* list, int64_t obj, int64_t location, int offset_x, int offset_y);
bool objlist_load(SectorObjectList* list, TigFile* stream, int64_t loc);
bool objlist_load_with_difs(SectorObjectList* list, TigFile* sec_stream, TigFile* dif_stream, int64_t sector_id, bool* base_mismatch);
bool objlist_save(SectorObjectList* list, TigFile* stream);
bool objlist_is_modified(SectorObjectList* list);
bool objlist_save_with_dif(SectorObjectList* list, TigFile* stream);
bool objlist_tile_of_object(SectorObjectList* list, int64_t obj, int* tile_ptr);
void objlist_precache_art(SectorObjectList* list);
void objlist_fold(SectorObjectList* list, int64_t location, int a4);
void sub_4F2230(int64_t obj, int* a2, int* a3);
void objlist_notify_sector_changed(int64_t sec, int64_t pc_obj);

#endif /* ARCANUM_GAME_SECTOR_OBJECT_LIST_H_ */
