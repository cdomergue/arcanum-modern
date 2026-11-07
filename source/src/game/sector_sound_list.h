#ifndef ARCANUM_GAME_SOUND_LIST_H_
#define ARCANUM_GAME_SOUND_LIST_H_

#include <tig/tig.h>

typedef struct SectorSoundList {
    /* 0000 */ unsigned int flags;
    /* 0004 */ int music_scheme_idx;
    /* 0008 */ int ambient_scheme_idx;
} SectorSoundList;

// Serializeable.
static_assert(sizeof(SectorSoundList) == 0xC, "wrong size");

bool sector_sound_list_init(SectorSoundList* list);
bool sector_sound_list_reset(SectorSoundList* list);
bool sector_sound_list_exit(SectorSoundList* list);
bool sector_sound_list_load(SectorSoundList* list, TigFile* stream);
bool sector_sound_list_save(SectorSoundList* list, TigFile* stream);
bool sector_sound_list_is_modified(SectorSoundList* list);
bool sector_sound_list_load_with_dif(SectorSoundList* list, TigFile* stream);
bool sector_sound_list_save_with_dif(SectorSoundList* list, TigFile* stream);

#endif /* ARCANUM_GAME_SOUND_LIST_H_ */
