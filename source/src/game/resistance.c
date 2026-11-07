#include "game/resistance.h"

// 0x5BC428
const char* resistance_lookup_keys_tbl[RESISTANCE_TYPE_COUNT] = {
    /*     RESISTANCE_TYPE_NORMAL */ "resist_damage",
    /*       RESISTANCE_TYPE_FIRE */ "resist_fire",
    /* RESISTANCE_TYPE_ELECTRICAL */ "resist_electrical",
    /*     RESISTANCE_TYPE_POISON */ "resist_poison",
    /*      RESISTANCE_TYPE_MAGIC */ "resist_magic",
};
