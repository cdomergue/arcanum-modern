#include "game/sector_roof_list.h"

#include "game/sector.h"

static bool sector_roof_list_load_internal(SectorRoofList* list, TigFile* stream);
static bool sector_roof_list_save_internal(SectorRoofList* list, TigFile* stream);

/**
 * Initializes a roof list.
 *
 * 0x4F7F20
 */
bool sector_roof_list_init(SectorRoofList* list)
{
    int index;

    for (index = 0; index < SECTOR_ROOF_LIST_SIZE; index++) {
        list->art_ids[index] = TIG_ART_ID_INVALID;
    }

    list->empty = true;

    return true;
}

/**
 * Resets a roof list.
 *
 * 0x4F7F40
 */
bool sector_roof_list_reset(SectorRoofList* list)
{
    int index;

    for (index = 0; index < SECTOR_ROOF_LIST_SIZE; index++) {
        list->art_ids[index] = TIG_ART_ID_INVALID;
    }

    list->empty = true;

    return true;
}

/**
 * Frees a roof list.
 *
 * NOTE: This is a no-op, probably included by the original devs for lifecycle
 * consistency.
 *
 * 0x4F7F70
 */
bool sector_roof_list_exit(SectorRoofList* list)
{
    (void)list;

    return true;
}

/**
 * Loads a roof list from a file.
 *
 * 0x4F7F80
 */
bool sector_roof_list_load(SectorRoofList* list, TigFile* stream)
{
    return sector_roof_list_load_internal(list, stream);
}

/**
 * Saves a roof list to a file.
 *
 * 0x4F7FA0
 */
bool sector_roof_list_save(SectorRoofList* list, TigFile* stream)
{
    return sector_roof_list_save_internal(list, stream);
}

/**
 * Checks if the roof list has been modified.
 *
 * NOTE: The roof list does not have modification tracking, therefore this
 * function always returns `false`. I haven't checked the editor.
 *
 * 0x4F7FC0
 */
bool sector_roof_list_is_modified(SectorRoofList* list)
{
    (void)list;

    return false;
}

/**
 * Loads updates for a roof list from a dif file.
 *
 * NOTE: This is a no-op, probably included by the original devs for lifecycle
 * consistency.
 *
 * 0x4F7FD0
 */
bool sector_roof_list_load_with_dif(SectorRoofList* list, TigFile* stream)
{
    (void)list;
    (void)stream;

    return true;
}

/**
 * Saves updates for a roof list to a dif file.
 *
 * NOTE: This is a no-op, probably included by the original devs for lifecycle
 * consistency.
 *
 * 0x4F7FE0
 */
bool sector_roof_list_save_with_dif(SectorRoofList* list, TigFile* stream)
{
    (void)list;
    (void)stream;

    return true;
}

/**
 * Registers all roof art IDs for preloading.
 *
 * 0x4F7FF0
 */
void sector_roof_list_precache_art(SectorRoofList* list)
{
    int index;

    for (index = 0; index < SECTOR_ROOF_LIST_SIZE; index++) {
        sector_art_cache_register(list->art_ids[index]);
    }
}

/**
 * Internal helper to load a roof list from a file.
 *
 * 0x4F8020
 */
bool sector_roof_list_load_internal(SectorRoofList* list, TigFile* stream)
{
    int index;

    if (tig_file_fread(&(list->empty), sizeof(list->empty), 1, stream) != 1) {
        return false;
    }

    if (!list->empty) {
        // There are roofs in the sector, load art IDs.
        if (tig_file_fread(list->art_ids, sizeof(*list->art_ids), SECTOR_ROOF_LIST_SIZE, stream) != SECTOR_ROOF_LIST_SIZE) {
            return false;
        }
    } else {
        // Nullify art IDs.
        for (index = 0; index < SECTOR_ROOF_LIST_SIZE; index++) {
            list->art_ids[index] = TIG_ART_ID_INVALID;
        }

        list->empty = true;
    }

    return true;
}

/**
 * Internal helper to save a roof list to a file.
 *
 * 0x4F8090
 */
bool sector_roof_list_save_internal(SectorRoofList* list, TigFile* stream)
{
    if (tig_file_fwrite(&(list->empty), sizeof(list->empty), 1, stream) != 1) {
        return false;
    }

    if (!list->empty) {
        if (tig_file_fwrite(list->art_ids, sizeof(*list->art_ids), SECTOR_ROOF_LIST_SIZE, stream) != SECTOR_ROOF_LIST_SIZE) {
            return false;
        }
    }

    return true;
}
