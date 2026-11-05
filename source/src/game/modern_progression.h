#ifndef ARCANUM_GAME_MODERN_PROGRESSION_H_
#define ARCANUM_GAME_MODERN_PROGRESSION_H_

#include <stdbool.h>
#include <stdint.h>

void modern_progression_reset(void);
void modern_progression_tick(void);
bool modern_progression_save(void);
bool modern_progression_load(void);
void modern_combat_xp_add(int64_t actor, int amount);
bool modern_combat_xp_sharing(void);
void modern_encounter_started(void);
void modern_encounter_travel(bool moving);
void modern_encounter_advance(unsigned milliseconds);
unsigned modern_encounter_grace_remaining(void);

#endif
