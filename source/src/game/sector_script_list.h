#ifndef ARCANUM_GAME_SECTOR_SCRIPT_LIST_H_
#define ARCANUM_GAME_SECTOR_SCRIPT_LIST_H_

#include "game/script.h"

#define SECTOR_SCRIPT_LIST_MODIFIED 0x0001

typedef struct SectorScriptList {
    /* 0000 */ unsigned int flags;
    /* 0004 */ Script scr;
} SectorScriptList;

bool sector_script_list_init(SectorScriptList* list);
bool sector_script_list_reset(SectorScriptList* list);
bool sector_script_list_exit(SectorScriptList* list);
bool sector_script_list_set(SectorScriptList* list, Script* scr);
bool sector_script_list_get(SectorScriptList* list, Script* scr);
bool sector_script_list_remove(SectorScriptList* list);
bool sector_script_list_load(SectorScriptList* list, TigFile* stream);
bool sector_script_list_save(SectorScriptList* list, TigFile* stream);
bool sector_script_list_is_modified(SectorScriptList* list);
bool sector_script_list_load_with_dif(SectorScriptList* list, TigFile* stream);
bool sector_script_list_save_with_dif(SectorScriptList* list, TigFile* stream);

#endif /* ARCANUM_GAME_SECTOR_SCRIPT_LIST_H_ */
