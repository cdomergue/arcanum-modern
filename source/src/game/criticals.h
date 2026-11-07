#ifndef ARCANUM_GAME_CRITICALS_H_
#define ARCANUM_GAME_CRITICALS_H_

/**
 * Defines the types of critical hits for weapons.
 *
 * These types are stored in `OBJ_F_WEAPON_CRIT_HIT_CHART` field. Refer to
 * `combat_process_crit_hit` for details on the effects.
 */
typedef enum CritHitType {
    CRIT_HIT_TYPE_CUTTING,
    CRIT_HIT_TYPE_CRUSHING,
    CRIT_HIT_TYPE_IMPALING,
    CRIT_HIT_TYPE_ELECTRICAL,
    CRIT_HIT_TYPE_FIRE,
    CRIT_HIT_TYPE_COUNT,
} CritHitType;

/**
 * Defines the types of critical misses for weapons.
 *
 * These types are stored in `OBJ_F_WEAPON_CRIT_MISS_CHART` field. Refer to
 * `combat_process_crit_miss` for details on the effects.
 */
typedef enum CritMissType {
    CRIT_MISS_TYPE_BLADED,
    CRIT_MISS_TYPE_BLUDGEON,
    CRIT_MISS_TYPE_HANDLED,
    CRIT_MISS_TYPE_GUNS,
    CRIT_MISS_TYPE_BOWS,
    CRIT_MISS_TYPE_HANDS,
    CRIT_MISS_TYPE_EXPLOSIVE,
    CRIT_MISS_TYPE_FIRE,
    CRIT_MISS_TYPE_ELECTRICAL,
    CRIT_MISS_TYPE_COUNT,
} CritMissType;

/**
 * Defines the body types of critters for the purposes of critical hit
 * calculations.
 *
 * These types are stored in `OBJ_F_CRITTER_CRIT_HIT_CHART` field. They are used
 * to limit the available effects when a critical hit occurs, for example, only
 * biped critters can be knocked down. Refer to `combat_process_crit_hit` for
 * the specific checks performed.
 */
typedef enum CritBodyType {
    CRIT_BODY_TYPE_BIPED,
    CRIT_BODY_TYPE_QUADRUPED,
    CRIT_BODY_TYPE_AMORPHOUS,
    CRIT_BODY_TYPE_INSECT,
    CRIT_BODY_TYPE_SNAKE,
    CRIT_BODY_TYPE_AVIAN,
    CRIT_BODY_TYPE_COUNT,
} CritBodyType;

#endif /* ARCANUM_GAME_CRITICALS_H_ */
