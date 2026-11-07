#ifndef ARCANUM_GAME_DAMAGE_TYPE_H_
#define ARCANUM_GAME_DAMAGE_TYPE_H_

// NOTE: The values does not match ResistanceType enum, even though both
// have 5 members.
typedef enum DamageType {
    DAMAGE_TYPE_NORMAL,
    DAMAGE_TYPE_POISON,
    DAMAGE_TYPE_ELECTRICAL,
    DAMAGE_TYPE_FIRE,
    DAMAGE_TYPE_FATIGUE,
    DAMAGE_TYPE_COUNT,
} DamageType;

#endif /* ARCANUM_GAME_DAMAGE_TYPE_H_ */
