#ifndef ARCANUM_GAME_MODERN_COMPAT_H_
#define ARCANUM_GAME_MODERN_COMPAT_H_

#include <stdbool.h>

typedef struct ModernCompatibility {
    bool uap;
    bool races;
    bool master_educator;
    bool no_combat_music;
    bool no_xp_per_hit;
    int level_limit;
} ModernCompatibility;

void modern_compat_load(void);
const ModernCompatibility* modern_compat_get(void);
const char* modern_compat_race_prefix(int race);

#endif
