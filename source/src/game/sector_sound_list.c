#include "game/sector_sound_list.h"

#define SECTOR_SOUND_LIST_MODIFIED 0x8000000

static void sector_sound_list_zero(SectorSoundList* list);

/**
 * Initializes a sound list.
 *
 * 0x4F80E0
 */
bool sector_sound_list_init(SectorSoundList* list)
{
    sector_sound_list_zero(list);

    return true;
}

/**
 * Resets a sound list.
 *
 * 0x4F8100
 */
bool sector_sound_list_reset(SectorSoundList* list)
{
    sector_sound_list_zero(list);

    return true;
}

/**
 * Internal helper that sets all sound list properties to zero.
 *
 * 0x4F8120
 */
void sector_sound_list_zero(SectorSoundList* list)
{
    list->flags = 0;
    list->music_scheme_idx = 0;
    list->ambient_scheme_idx = 0;
}

/**
 * Frees a sound list.
 *
 * 0x4F8130
 */
bool sector_sound_list_exit(SectorSoundList* list)
{
    sector_sound_list_zero(list);

    return true;
}

/**
 * Loads a sound list from a file.
 *
 * 0x4F8150
 */
bool sector_sound_list_load(SectorSoundList* list, TigFile* stream)
{
    if (tig_file_fread(list, sizeof(*list), 1, stream) != 1) {
        return false;
    }

    list->flags &= ~SECTOR_SOUND_LIST_MODIFIED;

    return true;
}

/**
 * Saves a sound list to a file.
 *
 * 0x4F8180
 */
bool sector_sound_list_save(SectorSoundList* list, TigFile* stream)
{
    list->flags &= ~SECTOR_SOUND_LIST_MODIFIED;

    if (tig_file_fwrite(list, sizeof(*list), 1, stream) != 1) {
        return false;
    }

    return true;
}

/**
 * Checks if the sound list has been modified.
 *
 * 0x4F81B0
 */
bool sector_sound_list_is_modified(SectorSoundList* list)
{
    return (list->flags & SECTOR_SOUND_LIST_MODIFIED) != 0;
}

/**
 * Loads updates for a sound list from a dif file.
 *
 * NOTE: This is a no-op, probably included by the original devs for lifecycle
 * consistency.
 *
 * 0x4F81C0
 */
bool sector_sound_list_load_with_dif(SectorSoundList* list, TigFile* stream)
{
    (void)list;
    (void)stream;

    return true;
}

/**
 * Saves updates for a sound list from a dif file.
 *
 * NOTE: This is a no-op, probably included by the original devs for lifecycle
 * consistency.
 *
 * 0x4F81D0
 */
bool sector_sound_list_save_with_dif(SectorSoundList* list, TigFile* stream)
{
    (void)list;
    (void)stream;

    return true;
}
