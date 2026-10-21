#ifndef ARCANUM_GAME_MODERN_AUTOSAVE_H_
#define ARCANUM_GAME_MODERN_AUTOSAVE_H_

#include <stdbool.h>

bool modern_autosave_is_slot(const char* name);
void modern_autosave_reset(void);
void modern_autosave_before_combat(void);
void modern_autosave_after_combat(void);
void modern_autosave_before_map_change(void);
void modern_autosave_tick(void);

#endif
