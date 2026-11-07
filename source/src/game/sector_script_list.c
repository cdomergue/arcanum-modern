#include "game/sector_script_list.h"

/**
 * Initializes a script list.
 *
 * 0x4F6170
 */
bool sector_script_list_init(SectorScriptList* list)
{
    return sector_script_list_reset(list);
}

/**
 * Resets a script list.
 *
 * 0x4F6180
 */
bool sector_script_list_reset(SectorScriptList* list)
{
    list->flags = 0;
    memset(&(list->scr), 0, sizeof(list->scr));
    list->flags = 0;
    list->scr.num = 0;

    return true;
}

/**
 * Frees a script list.
 *
 * 0x4F61B0
 */
bool sector_script_list_exit(SectorScriptList* list)
{
    return sector_script_list_reset(list);
}

/**
 * Sets the script to a script list.
 *
 * 0x4F61C0
 */
bool sector_script_list_set(SectorScriptList* list, Script* scr)
{
    list->scr = *scr;
    list->flags |= SECTOR_SCRIPT_LIST_MODIFIED;
    return true;
}

/**
 * Retrieves the script from a script list.
 *
 * 0x4F61F0
 */
bool sector_script_list_get(SectorScriptList* list, Script* scr)
{
    *scr = list->scr;

    return scr->num != 0;
}

/**
 * Clears script list.
 *
 * 0x4F6220
 */
bool sector_script_list_remove(SectorScriptList* list)
{
    if (list->scr.num != 0) {
        sector_script_list_reset(list);
        list->flags |= SECTOR_SCRIPT_LIST_MODIFIED;
    }

    return true;
}

/**
 * Loads a script list from a file.
 *
 * 0x4F6250
 */
bool sector_script_list_load(SectorScriptList* list, TigFile* stream)
{
    if (tig_file_fread(&(list->scr), sizeof(list->scr), 1, stream) != 1) {
        return false;
    }

    return true;
}

/**
 * Saves a script list to a file.
 *
 * 0x4F6270
 */
bool sector_script_list_save(SectorScriptList* list, TigFile* stream)
{
    if (tig_file_fwrite(&(list->scr), sizeof(list->scr), 1, stream) != 1) {
        return false;
    }

    list->flags &= ~SECTOR_SCRIPT_LIST_MODIFIED;

    return true;
}

/**
 * Checks if the script list has been modified.
 *
 * 0x4F62A0
 */
bool sector_script_list_is_modified(SectorScriptList* list)
{
    return (list->flags & SECTOR_SCRIPT_LIST_MODIFIED) != 0;
}

/**
 * Loads updates for a script list from a dif file.
 *
 * 0x4F62B0
 */
bool sector_script_list_load_with_dif(SectorScriptList* list, TigFile* stream)
{
    if (!sector_script_list_load(list, stream)) {
        return false;
    }

    list->flags |= SECTOR_SCRIPT_LIST_MODIFIED;

    return true;
}

/**
 * Saves updates for a script list to a dif file.
 *
 * 0x4F62E0
 */
bool sector_script_list_save_with_dif(SectorScriptList* list, TigFile* stream)
{
    if (!sector_script_list_save(list, stream)) {
        return false;
    }

    // FIXME: This looks wrong. This flag should be unset after script list is
    // saved.
    list->flags |= SECTOR_SCRIPT_LIST_MODIFIED;

    return true;
}
