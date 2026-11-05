#ifndef MODERN_CAMPAIGN_FIXES_H
#define MODERN_CAMPAIGN_FIXES_H

#include <string.h>

/* Willoughsby's negotiation reward marks quest 1098 complete on the NPC line,
 * but sets the dialogue's local exit flag only on a later player response.
 * Re-entering after interruption must still allow the training dialogue. Skip
 * only the money action when the native quest record already says completed.
 * Exact action matching leaves modified rewards and unrelated campaigns alone.
 * No additional state is serialized, including for existing campaigns. */
static inline const char* modern_campaign_reward_actions(const char* module, int script,
    int line, int quest_state, const char* actions)
{
    if (!module || strcmp(module, "Arcanum") != 0 || script != 1565
        || quest_state != 4 || !actions) {
        return actions;
    }
    if (line == 50 && strcmp(actions, "$$7000, qu1098 4") == 0) {
        return actions + 8;
    }
    if (line == 73 && strcmp(actions, "$$11000, rp 1044, qu1098 4") == 0) {
        return actions + 9;
    }
    return actions;
}

/* One native Tarant sewer instance overrides its mechanical prototype's flags.
 * Restore only the missing mechanical bit. Keep every other instance override
 * and use the existing native flag, which CE can read without this correction.
 * These are the GUID bytes stored in the MOB, not its filename's byte order. */
static inline unsigned modern_campaign_critter_flags(const char* module,
    int prototype, const unsigned char* guid, unsigned flags)
{
    static const unsigned char arachnid[16] = {
        0x47, 0xF7, 0xC5, 0xDE, 0x80, 0x15, 0xD4, 0x11,
        0x8F, 0x1D, 0x00, 0xE0, 0x29, 0x20, 0x22, 0x0C,
    };
    if (module && strcmp(module, "Arcanum") == 0 && prototype == 28343
        && guid && memcmp(guid, arachnid, sizeof(arachnid)) == 0) {
        return flags | 0x20000000u; /* OCF_MECHANICAL */
    }
    return flags;
}

#endif
