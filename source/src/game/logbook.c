#include "game/logbook.h"

#include "game/critter.h"
#include "game/obj.h"
#include "game/stat.h"

#define FIRST_INJURY_RECORD_IDX 64

/**
 * Updates the logbook with a new kill, tracking total kills and notable enemy
 * stats.
 *
 * NOTE: All compares are signed even though `OBJ_F_PC_LOGBOOK_EGO_IDX` is
 * designated as unsigned.
 *
 * 0x4F5330
 */
void logbook_add_kill(int64_t pc_obj, int64_t npc_obj)
{
    int desc;
    int value;

    // Retrieve the description ID of the NPC as known by PC.
    desc = critter_description_get(npc_obj, pc_obj);

    // Increment total kills in the logbook.
    value = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_TOTAL_KILLS);
    obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_TOTAL_KILLS, value + 1);

    // Update most and least powerful enemies.
    value = stat_level_get(npc_obj, STAT_LEVEL);
    if (value >= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_LEVEL)
        || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_NAME) == 0) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_LEVEL, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_NAME, desc);
    }
    if (value <= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_LEVEL)
        || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_NAME) == 0) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_LEVEL, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_NAME, desc);
    }

    // Update good/evil enemy metrics.
    value = stat_level_get(npc_obj, STAT_ALIGNMENT);
    if (value > 0
        && (value >= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_VALUE)
            || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_NAME) == 0)) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_VALUE, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_NAME, desc);
    }
    if (value < 0
        && (value <= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_VALUE)
            || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_NAME) == 0)) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_VALUE, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_NAME, desc);
    }

    // Update magical/technological enemy metrics.
    value = stat_level_get(npc_obj, STAT_MAGICK_TECH_APTITUDE);
    if (value > 0
        && (value >= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_VALUE)
            || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_NAME) == 0)) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_VALUE, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_NAME, desc);
    }
    if (value < 0
        && (value <= (int)obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_VALUE)
            || obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_NAME) == 0)) {
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_VALUE, value);
        obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_NAME, desc);
    }
}

/**
 * Retrieves kill-related statistics from the logbook.
 *
 * The `values` array must contain it least `LBK_COUNT`elements.
 *
 * 0x4F5590
 */
void logbook_get_kills(int64_t pc_obj, int* values)
{
    values[LBK_TOTAL_KILLS] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_TOTAL_KILLS);
    values[LBK_MOST_POWERFUL_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_NAME);
    values[LBK_MOST_POWERFUL_LEVEL] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_POWERFUL_LEVEL);
    values[LBK_LEAST_POWERFUL_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_NAME);
    values[LBK_LEAST_POWERFUL_LEVEL] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_LEAST_POWERFUL_LEVEL);
    values[LBK_MOST_GOOD_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_NAME);
    values[LBK_MOST_GOOD_VALUE] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_GOOD_VALUE);
    values[LBK_MOST_EVIL_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_NAME);
    values[LBK_MOST_EVIL_VALUE] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_EVIL_VALUE);
    values[LBK_MOST_MAGICAL_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_NAME);
    values[LBK_MOST_MAGICAL_VALUE] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_MAGICAL_VALUE);
    values[LBK_MOST_TECH_NAME] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_NAME);
    values[LBK_MOST_TECH_VALUE] = obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, LBK_MOST_TECH_VALUE);
}

/**
 * Records an injury inflicted to a player character in the logbook.
 *
 * 0x4F5690
 */
void logbook_add_injury(int64_t pc_obj, int64_t inflicted_by_obj, int injury)
{
    int type;
    int desc;
    int idx;

    // Ensure attacker is present.
    if (inflicted_by_obj == OBJ_HANDLE_NULL) {
        return;
    }

    // Only PC objects have logbook entries.
    if (obj_field_int32_get(pc_obj, OBJ_F_TYPE) != OBJ_TYPE_PC) {
        return;
    }

    // Only record injuries inflicted by critters.
    type = obj_field_int32_get(inflicted_by_obj, OBJ_F_TYPE);
    if (obj_type_is_critter(type)) {
        return;
    }

    // Determine the description based on who inflicted the injury.
    if (pc_obj == inflicted_by_obj) {
        desc = 30286; // "yourself (self-inflicted)"
    } else if (type == OBJ_TYPE_PC) {
        desc = 30287; // "another Player"
    } else {
        // Retrieve the description ID of the NPC as known by PC.
        desc = critter_description_get(inflicted_by_obj, pc_obj);
    }

    // Find the next available slot for injury data.
    idx = FIRST_INJURY_RECORD_IDX;
    while (obj_arrayfield_uint32_get(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx) != 0) {
        idx += 2;
    }

    // Store the description of critter who inflicted the injury and type of
    // injury.
    obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx, desc);
    obj_arrayfield_uint32_set(pc_obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx + 1, injury);
}

/**
 * Retrieves the first injury record for a player character.
 *
 * 0x4F5770
 */
int logbook_find_first_injury(int64_t obj, int* desc_ptr, int* injury_ptr)
{
    int idx = FIRST_INJURY_RECORD_IDX;

    // Retrieve the description of the critter that inflicted the injury.
    *desc_ptr = obj_arrayfield_uint32_get(obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx);
    if (*desc_ptr == 0) {
        return 0;
    }

    // Retrieve the injury type.
    *injury_ptr = obj_arrayfield_uint32_get(obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx + 1);

    return idx;
}

/**
 * Continues iteration over injury records for a player character.
 *
 * 0x4F57C0
 */
int logbook_find_next_injury(int64_t obj, int idx, int* desc_ptr, int* injury_ptr)
{
    idx += 2;

    // Retrieve the description of the critter that inflicted the injury.
    *desc_ptr = obj_arrayfield_uint32_get(obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx);
    if (*desc_ptr == 0) {
        return 0;
    }

    // Retrieve the injury type.
    *injury_ptr = obj_arrayfield_uint32_get(obj, OBJ_F_PC_LOGBOOK_EGO_IDX, idx + 1);

    return idx;
}
