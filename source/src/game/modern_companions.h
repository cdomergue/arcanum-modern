#ifndef ARCANUM_GAME_MODERN_COMPANIONS_H_
#define ARCANUM_GAME_MODERN_COMPANIONS_H_
#include "game/obj.h"

enum { COMPANION_PRESENT=1, COMPANION_WAITING, COMPANION_LEFT,
    COMPANION_WAIT_EXPIRED, COMPANION_DEAD, COMPANION_UNAVAILABLE };
typedef struct CompanionEntry {
    ObjectID id;
    int description, map, state, recovered;
    int64_t location;
} CompanionEntry;
void modern_companions_reset(void);
void modern_companions_refresh(void);
void modern_companions_event(int64_t npc, int64_t leader, int state);
bool modern_companions_save(void);
int modern_companions_count(void);
const CompanionEntry* modern_companions_get(int index);
/* Map metadata only; no object instantiation, map loading or script execution. */
void modern_companions_scan(void);
void modern_companions_import(const CompanionSnapshot* snapshot, int map);
bool modern_companions_coordinates(int map, int64_t location, int* west, int* south);
#endif
