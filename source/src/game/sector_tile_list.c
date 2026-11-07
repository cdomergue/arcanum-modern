#include "game/sector_tile_list.h"

#include <stdio.h>

#include "game/sector.h"

static bool sector_tile_list_load_internal(SectorTileList* list, TigFile* stream);
static bool sector_tile_list_save_internal(SectorTileList* list, TigFile* stream);
static bool sector_tile_list_load_with_dif_internal(SectorTileList* list, TigFile* stream);
static bool sector_tile_list_save_with_dif_internal(SectorTileList* list, TigFile* stream);

/**
 * Initializes a tile list with a default art ID.
 *
 * NOTE: Unclear why art ID is passed as pointer.
 *
 * 0x4F7BA0
 */
bool sector_tile_list_init(SectorTileList* list, tig_art_id_t* art_id_ptr)
{
    int index;

    for (index = 0; index < 4096; index++) {
        list->art_ids[index] = *art_id_ptr;
    }

    memset(list->difmask, 0, sizeof(list->difmask));
    list->dif = false;

    return true;
}

/**
 * Resets a tile list.
 *
 * 0x4F7BE0
 */
bool sector_tile_list_reset(SectorTileList* list)
{
    // FIXME: Why 32?
    memset(list->difmask, 0, 32);
    list->dif = false;

    return true;
}

/**
 * Frees a tile list.
 *
 * 0x4F7C20
 */
bool sector_tile_list_exit(SectorTileList* list)
{
    (void)list;

    return true;
}

/**
 * Loads a tile list from a file.
 *
 * 0x4F7C30
 */
bool sector_tile_list_load(SectorTileList* list, TigFile* stream)
{
    return sector_tile_list_load_internal(list, stream);
}

/**
 * Saves a tile list to a file.
 *
 * 0x4F7C50
 */
bool sector_tile_list_save(SectorTileList* list, TigFile* stream)
{
    if (!sector_tile_list_save_internal(list, stream)) {
        return false;
    }

    list->dif = false;

    return true;
}

/**
 * Loads updates for a tile list from a dif file.
 *
 * 0x4F7C80
 */
bool sector_tile_list_load_with_dif(SectorTileList* list, TigFile* stream)
{
    return sector_tile_list_load_with_dif_internal(list, stream);
}

/**
 * Saves updates for a tile list to a dif file.
 *
 * 0x4F7CA0
 */
bool sector_tile_list_save_with_dif(SectorTileList* list, TigFile* stream)
{
    return sector_tile_list_save_with_dif_internal(list, stream);
}

/**
 * Registers all tile art IDs for preloading.
 *
 * 0x4F7CC0
 */
void sector_list_tile_precache_art(SectorTileList* list)
{
    int index;

    for (index = 0; index < 4096; index++) {
        sector_art_cache_register(list->art_ids[index]);
    }
}

/**
 * Internal helper to load a tile list from a file.
 *
 * 0x4F7CE0
 */
bool sector_tile_list_load_internal(SectorTileList* list, TigFile* stream)
{
    if (tig_file_fread(list->art_ids, sizeof(*list->art_ids), 4096, stream) != 4096) {
        return false;
    }

    return true;
}

/**
 * Internal helper to save a roof list to a file.
 *
 * 0x4F7D10
 */
bool sector_tile_list_save_internal(SectorTileList* list, TigFile* stream)
{
    if (tig_file_fwrite(list->art_ids, sizeof(*list->art_ids), 4096, stream) != 4096) {
        return false;
    }

    return true;
}

/**
 * Internal helper to load changes to a tile list from a dif file.
 *
 * 0x4F7D40
 */
bool sector_tile_list_load_with_dif_internal(SectorTileList* list, TigFile* stream)
{
    int cnt;
    int index;
    int tile;

    // Read the number of changed tiles.
    if (tig_file_fread(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    // Process each change entry.
    for (index = 0; index < cnt; index++) {
        // Read the index of the tile.
        if (tig_file_fread(&tile, sizeof(tile), 1, stream) != 1) {
            return false;
        }

        // Read the new art ID.
        if (tig_file_fread(&(list->art_ids[tile]), sizeof(list->art_ids[tile]), 1, stream) != 1) {
            return false;
        }

        // Mark appopriate tile as changed.
        list->difmask[tile / 32] |= 1 << (tile % 32);
        list->dif = true;
    }

    return true;
}

/**
 * Internal helper to save changes to a tile list from a dif file.
 *
 * 0x4F7DF0
 */
bool sector_tile_list_save_with_dif_internal(SectorTileList* list, TigFile* stream)
{
    int cnt_pos;
    int cnt;
    int tile;
    int pos;

    // Remember the position in file stream where number of change entries
    // should be written at.
    cnt_pos = tig_file_ftell(stream);
    if (cnt_pos == -1) {
        return false;
    }

    // Write a placeholder.
    cnt = 0;
    if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
        return false;
    }

    // Iterate through all tiles to find those marked as changed.
    for (tile = 0; tile < 4096; tile++) {
        if ((list->difmask[tile / 32] & (1 << (tile % 32))) != 0) {
            // Write the index of the tile.
            if (tig_file_fwrite(&tile, sizeof(tile), 1, stream) != 1) {
                return false;
            }

            // Write changed art ID.
            if (tig_file_fwrite(&(list->art_ids[tile]), sizeof(list->art_ids[tile]), 1, stream) != 1) {
                return false;
            }

            cnt++;
        }
    }

    // Check if there were any changes.
    if (cnt != 0) {
        // Remember the current position.
        pos = tig_file_ftell(stream);
        if (pos == -1) {
            return false;
        }

        // Seek back to the number of change entries position.
        if (tig_file_fseek(stream, cnt_pos, SEEK_SET) != 0) {
            return false;
        }

        // Overwrite the placeholder with the actual count.
        if (tig_file_fwrite(&cnt, sizeof(cnt), 1, stream) != 1) {
            return false;
        }

        // Restore the stream position.
        if (tig_file_fseek(stream, pos, SEEK_SET) != 0) {
            return false;
        }
    }

    return true;
}
