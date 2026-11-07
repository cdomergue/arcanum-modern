#ifndef ARCANUM_GAME_OBJ_FLAGS_H_
#define ARCANUM_GAME_OBJ_FLAGS_H_

#include <stdint.h>

typedef uint32_t ObjectFlags;

#define OF_COUNT 31

// clang-format off
#define OF_DESTROYED        0x00000001u
#define OF_OFF              0x00000002u
#define OF_FLAT             0x00000004u
#define OF_TEXT             0x00000008u
#define OF_SEE_THROUGH      0x00000010u
#define OF_SHOOT_THROUGH    0x00000020u
#define OF_TRANSLUCENT      0x00000040u
#define OF_SHRUNK           0x00000080u
#define OF_DONTDRAW         0x00000100u
#define OF_INVISIBLE        0x00000200u
#define OF_NO_BLOCK         0x00000400u
#define OF_CLICK_THROUGH    0x00000800u
#define OF_INVENTORY        0x00001000u
#define OF_DYNAMIC          0x00002000u
#define OF_PROVIDES_COVER   0x00004000u
#define OF_HAS_OVERLAYS     0x00008000u
#define OF_HAS_UNDERLAYS    0x00010000u
#define OF_WADING           0x00020000u
#define OF_WATER_WALKING    0x00040000u
#define OF_STONED           0x00080000u
#define OF_DONTLIGHT        0x00100000u
#define OF_TEXT_FLOATER     0x00200000u
#define OF_INVULNERABLE     0x00400000u
#define OF_EXTINCT          0x00800000u
#define OF_TRAP_PC          0x01000000u
#define OF_TRAP_SPOTTED     0x02000000u
#define OF_DISALLOW_WADING  0x04000000u
#define OF_MULTIPLAYER_LOCK 0x08000000u
#define OF_FROZEN           0x10000000u
#define OF_ANIMATED_DEAD    0x20000000u
#define OF_TELEPORTED       0x40000000u
// clang-format on

typedef uint32_t ObjectSpellFlags;

#define OSF_COUNT 32

// clang-format off
#define OSF_INVISIBLE           0x00000001u
#define OSF_FLOATING            0x00000002u
#define OSF_BODY_OF_AIR         0x00000004u
#define OSF_BODY_OF_EARTH       0x00000008u
#define OSF_BODY_OF_FIRE        0x00000010u
#define OSF_BODY_OF_WATER       0x00000020u
#define OSF_DETECTING_MAGIC     0x00000040u
#define OSF_DETECTING_ALIGNMENT 0x00000080u
#define OSF_DETECTING_TRAPS     0x00000100u
#define OSF_DETECTING_INVISIBLE 0x00000200u
#define OSF_SHIELDED            0x00000400u
#define OSF_ANTI_MAGIC_SHELL    0x00000800u
#define OSF_BONDS_OF_MAGIC      0x00001000u
#define OSF_FULL_REFLECTION     0x00002000u
#define OSF_SUMMONED            0x00004000u
#define OSF_ILLUSION            0x00008000u
#define OSF_STONED              0x00010000u
#define OSF_POLYMORPHED         0x00020000u
#define OSF_MIRRORED            0x00040000u
#define OSF_SHRUNK              0x00080000u
#define OSF_PASSWALLED          0x00100000u
#define OSF_WATER_WALKING       0x00200000u
#define OSF_MAGNETIC_INVERSION  0x00400000u
#define OSF_CHARMED             0x00800000u
#define OSF_ENTANGLED           0x01000000u
#define OSF_SPOKEN_WITH_DEAD    0x02000000u
#define OSF_TEMPUS_FUGIT        0x04000000u
#define OSF_MIND_CONTROLLED     0x08000000u
#define OSF_DRUNK               0x10000000u
#define OSF_ENSHROUDED          0x20000000u
#define OSF_FAMILIAR            0x40000000u
#define OSF_HARDENED_HANDS      0x80000000u
// clang-format on

typedef uint32_t ObjectWallFlags;

#define OWAF_COUNT 4

// clang-format off
#define OWAF_TRANS_DISALLOW 0x0001u
#define OWAF_TRANS_LEFT     0x0002u
#define OWAF_TRANS_RIGHT    0x0004u
#define OWAF_TRANS_ALL      0x0008u
// clang-format on

typedef uint32_t ObjectPortalFlags;

#define OPF_COUNT 9

// clang-format off
#define OPF_LOCKED         0x0001u
#define OPF_JAMMED         0x0002u
#define OPF_MAGICALLY_HELD 0x0004u
#define OPF_NEVER_LOCKED   0x0008u
#define OPF_ALWAYS_LOCKED  0x0010u
#define OPF_LOCKED_DAY     0x0020u
#define OPF_LOCKED_NIGHT   0x0040u
#define OPF_BUSTED         0x0080u
#define OPF_STICKY         0x0100u
// clang-format on

typedef uint32_t ObjectContainerFlags;

#define OCOF_COUNT 9

// clang-format off
#define OCOF_LOCKED                  0x0001u
#define OCOF_JAMMED                  0x0002u
#define OCOF_MAGICALLY_HELD          0x0004u
#define OCOF_NEVER_LOCKED            0x0008u
#define OCOF_ALWAYS_LOCKED           0x0010u
#define OCOF_LOCKED_DAY              0x0020u
#define OCOF_LOCKED_NIGHT            0x0040u
#define OCOF_BUSTED                  0x0080u
#define OCOF_STICKY                  0x0100u
#define OCOF_INVEN_SPAWN_ONCE        0x0200u
#define OCOF_INVEN_SPAWN_INDEPENDENT 0x0400u
// clang-format on

typedef uint32_t ObjectSceneryFlags;

#define OSCF_COUNT 9

// clang-format off
#define OSCF_NO_AUTO_ANIMATE   0x0001u
#define OSCF_BUSTED            0x0002u
#define OSCF_NOCTURNAL         0x0004u
#define OSCF_MARKS_TOWNMAP     0x0008u
#define OSCF_IS_FIRE           0x0010u
#define OSCF_RESPAWNABLE       0x0020u
#define OSCF_SOUND_SMALL       0x0040u
#define OSCF_SOUND_MEDIUM      0x0080u
#define OSCF_SOUND_EXTRA_LARGE 0x0100u
#define OSCF_UNDER_ALL         0x0200u
#define OSCF_RESPAWNING        0x0400u
// clang-format on

typedef uint32_t ObjectItemFlags;

#define OIF_COUNT 23

// clang-format off
#define OIF_IDENTIFIED      0x00000001u
#define OIF_WONT_SELL       0x00000002u
#define OIF_IS_MAGICAL      0x00000004u
#define OIF_TRANSFER_LIGHT  0x00000008u
#define OIF_NO_DISPLAY      0x00000010u
#define OIF_NO_DROP         0x00000020u
#define OIF_HEXED           0x00000040u
#define OIF_CAN_USE_BOX     0x00000080u
#define OIF_NEEDS_TARGET    0x00000100u
#define OIF_LIGHT_SMALL     0x00000200u
#define OIF_LIGHT_MEDIUM    0x00000400u
#define OIF_LIGHT_LARGE     0x00000800u
#define OIF_LIGHT_XLARGE    0x00001000u
#define OIF_PERSISTENT      0x00002000u
#define OIF_MT_TRIGGERED    0x00004000u
#define OIF_STOLEN          0x00008000u
#define OIF_USE_IS_THROW    0x00010000u
#define OIF_NO_DECAY        0x00020000u
#define OIF_UBER            0x00040000u
#define OIF_NO_NPC_PICKUP   0x00080000u
#define OIF_NO_RANGED_USE   0x00100000u
#define OIF_VALID_AI_ACTION 0x00200000u
#define OIF_MP_INSERTED     0x00400000u
// clang-format on

#define OIF_LIGHT_ANY (OIF_LIGHT_XLARGE | OIF_LIGHT_LARGE | OIF_LIGHT_MEDIUM | OIF_LIGHT_SMALL)

typedef uint32_t ObjectWeaponFlags;

#define OWF_COUNT 10

// clang-format off
#define OWF_LOUD              0x0001u
#define OWF_SILENT            0x0002u
#define OWF_TWO_HANDED        0x0004u
#define OWF_HAND_COUNT_FIXED  0x0008u
#define OWF_THROWABLE         0x0010u
#define OWF_TRANS_PROJECTILE  0x0020u
#define OWF_BOOMERANGS        0x0040u
#define OWF_IGNORE_RESISTANCE 0x0080u
#define OWF_DAMAGE_ARMOR      0x0100u
#define OWF_DEFAULT_THROWS    0x0200u
// clang-format on

typedef uint32_t ObjectAmmoFlags;

#define OAF_COUNT 1

// clang-format off
#define OAF_NONE 0x0001u
// clang-format on

typedef uint32_t ObjectArmorFlags;

#define OARF_COUNT 5

// clang-format off
#define OARF_SIZE_SMALL  0x0001u
#define OARF_SIZE_MEDIUM 0x0002u
#define OARF_SIZE_LARGE  0x0004u
#define OARF_MALE_ONLY   0x0008u
#define OARF_FEMALE_ONLY 0x0010u
// clang-format on

typedef uint32_t ObjectGoldFlags;

#define OGOF_COUNT 1

// clang-format off
#define OGOF_NONE 0x0001u
// clang-format on

typedef uint32_t ObjectFoodFlags;

#define OFF_COUNT 1

// clang-format off
#define OFF_NONE 0x0001
// clang-format on

typedef uint32_t ObjectScrollFlags;

#define OSRF_COUNT 1

// clang-format off
#define OSRF_NONE 0x0001
// clang-format on

typedef uint32_t ObjectKeyRingFlags;

#define OKRF_COUNT 1

// clang-format off
#define OKRF_PRIMARY_RING 0x0001
// clang-format on

typedef uint32_t ObjectWrittenFlags;

#define OWRF_COUNT 1

// clang-format off
#define OWRF_NONE 0x0001u
// clang-format on

typedef uint32_t ObjectGenericFlags;

#define OGF_COUNT 5

// clang-format off
#define OGF_USES_TORCH_SHIELD_LOCATION 0x0001u
#define OGF_IS_LOCKPICK                0x0002u
#define OGF_IS_TRAP_DEVICE             0x0004u
#define OGF_IS_HEALING_ITEM            0x0008u
#define OGF_IS_GRENADE                 0x0010u
// clang-format on

typedef uint32_t ObjectCritterFlags;

#define OCF_COUNT 32

// clang-format off
#define OCF_IS_CONCEALED       0x00000001u
#define OCF_MOVING_SILENTLY    0x00000002u
#define OCF_UNDEAD             0x00000004u
#define OCF_ANIMAL             0x00000008u
#define OCF_FLEEING            0x00000010u
#define OCF_STUNNED            0x00000020u
#define OCF_PARALYZED          0x00000040u
#define OCF_BLINDED            0x00000080u
#define OCF_CRIPPLED_ARMS_ONE  0x00000100u
#define OCF_CRIPPLED_ARMS_BOTH 0x00000200u
#define OCF_CRIPPLED_LEGS_BOTH 0x00000400u
#define OCF_UNUSED             0x00000800u
#define OCF_SLEEPING           0x00001000u
#define OCF_MUTE               0x00002000u
#define OCF_SURRENDERED        0x00004000u
#define OCF_MONSTER            0x00008000u
#define OCF_SPELL_FLEE         0x00010000u
#define OCF_ENCOUNTER          0x00020000u
#define OCF_COMBAT_MODE_ACTIVE 0x00040000u
#define OCF_LIGHT_SMALL        0x00080000u
#define OCF_LIGHT_MEDIUM       0x00100000u
#define OCF_LIGHT_LARGE        0x00200000u
#define OCF_LIGHT_XLARGE       0x00400000u
#define OCF_UNREVIVIFIABLE     0x00800000u
#define OCF_UNRESSURECTABLE    0x01000000u
#define OCF_DEMON              0x02000000u
#define OCF_FATIGUE_IMMUNE     0x04000000u
#define OCF_NO_FLEE            0x08000000u
#define OCF_NON_LETHAL_COMBAT  0x10000000u
#define OCF_MECHANICAL         0x20000000u
#define OCF_ANIMAL_ENSHROUD    0x40000000u
#define OCF_FATIGUE_LIMITING   0x80000000u
// clang-format on

#define OCF_CRIPPLED (OCF_CRIPPLED_ARMS_ONE | OCF_CRIPPLED_ARMS_BOTH | OCF_CRIPPLED_LEGS_BOTH)
#define OCF_INJURED (OCF_BLINDED | OCF_CRIPPLED)
#define OCF_LIGHT_ANY (OCF_LIGHT_XLARGE | OCF_LIGHT_LARGE | OCF_LIGHT_MEDIUM | OCF_LIGHT_SMALL)

typedef uint32_t ObjectCritterFlags2;

#define OCF2_COUNT 27

// clang-format off
#define OCF2_ITEM_STOLEN        0x00000001u
#define OCF2_AUTO_ANIMATES      0x00000002u
#define OCF2_USING_BOOMERANG    0x00000004u
#define OCF2_FATIGUE_DRAINING   0x00000008u
#define OCF2_SLOW_PARTY         0x00000010u
#define OCF2_COMBAT_TOGGLE_FX   0x00000020u
#define OCF2_NO_DECAY           0x00000040u
#define OCF2_NO_PICKPOCKET      0x00000080u
#define OCF2_NO_BLOOD_SPLOTCHES 0x00000100u
#define OCF2_NIGH_INVULNERABLE  0x00000200u
#define OCF2_ELEMENTAL          0x00000400u
#define OCF2_DARK_SIGHT         0x00000800u
#define OCF2_NO_SLIP            0x00001000u
#define OCF2_NO_DISINTEGRATE    0x00002000u
#define OCF2_REACTION_0         0x00004000u
#define OCF2_REACTION_1         0x00008000u
#define OCF2_REACTION_2         0x00010000u
#define OCF2_REACTION_3         0x00020000u
#define OCF2_REACTION_4         0x00040000u
#define OCF2_REACTION_5         0x00080000u
#define OCF2_REACTION_6         0x00100000u
#define OCF2_TARGET_LOCK        0x00200000u
#define OCF2_PERMA_POLYMORPH    0x00400000u
#define OCF2_SAFE_OFF           0x00800000u
#define OCF2_CHECK_REACTION_BAD 0x01000000u
#define OCF2_CHECK_ALIGN_GOOD   0x02000000u
#define OCF2_CHECK_ALIGN_BAD    0x04000000u
// clang-format on

typedef uint32_t ObjectPcFlags;

#define OPCF_COUNT 6

// clang-format off
#define OPCF_unused_1           0x0001u
#define OPCF_unused_2           0x0002u
#define OPCF_USE_ALT_DATA       0x0004u
#define OPCF_unused_4           0x0008u
#define OPCF_unused_5           0x0010u
#define OPCF_FOLLOWER_SKILLS_ON 0x0020u
// clang-format on

typedef uint32_t ObjectNpcFlags;

#define ONF_COUNT 31

// clang-format off
#define ONF_FIGHTING          0x00000001u
#define ONF_WAYPOINTS_DAY     0x00000002u
#define ONF_WAYPOINTS_NIGHT   0x00000004u
#define ONF_AI_WAIT_HERE      0x00000008u
#define ONF_AI_SPREAD_OUT     0x00000010u
#define ONF_JILTED            0x00000020u
#define ONF_CHECK_WIELD       0x00000040u
#define ONF_CHECK_WEAPON      0x00000080u
#define ONF_KOS               0x00000100u
#define ONF_WAYPOINTS_BED     0x00000200u
#define ONF_FORCED_FOLLOWER   0x00000400u
#define ONF_KOS_OVERRIDE      0x00000800u
#define ONF_WANDERS           0x00001000u
#define ONF_WANDERS_IN_DARK   0x00002000u
#define ONF_FENCE             0x00004000u
#define ONF_FAMILIAR          0x00008000u
#define ONF_CHECK_LEADER      0x00010000u
#define ONF_ALOOF             0x00020000u
#define ONF_CAST_HIGHEST      0x00040000u
#define ONF_GENERATOR         0x00080000u
#define ONF_GENERATED         0x00100000u
#define ONF_GENERATOR_RATE1   0x00200000u
#define ONF_GENERATOR_RATE2   0x00400000u
#define ONF_GENERATOR_RATE3   0x00800000u
#define ONF_DEMAINTAIN_SPELLS 0x01000000u
#define ONF_LOOK_FOR_WEAPON   0x02000000u
#define ONF_LOOK_FOR_ARMOR    0x04000000u
#define ONF_LOOK_FOR_AMMO     0x08000000u
#define ONF_BACKING_OFF       0x10000000u
#define ONF_NO_ATTACK         0x20000000u
#define ONF_CHECK_GRENADE     0x40000000u
// clang-format on

typedef uint32_t ObjectTrapFlags;

#define OTF_COUNT 1

// clang-format off
#define OTF_BUSTED 0x0001u
// clang-format on

typedef uint32_t ObjectRenderFlags;

#define ORF_01000000 0x01000000
#define ORF_02000000 0x02000000
#define ORF_04000000 0x04000000
#define ORF_08000000 0x08000000
#define ORF_10000000 0x10000000
#define ORF_20000000 0x20000000
#define ORF_40000000 0x40000000
#define ORF_80000000 0x80000000

typedef enum ObjectFlagSet {
    OFS_FLAGS,
    OFS_SPELL_FLAGS,
    OFS_WALL_FLAGS,
    OFS_PORTAL_FLAGS,
    OFS_CONTAINER_FLAGS,
    OFS_SCENERY_FLAGS,
    OFS_ITEM_FLAGS,
    OFS_WEAPON_FLAGS,
    OFS_AMMO_FLAGS,
    OFS_ARMOR_FLAGS,
    OFS_GOLD_FLAGS,
    OFS_FOOD_FLAGS,
    OFS_SCROLL_FLAGS,
    OFS_KEY_RING_FLAGS,
    OFS_WRITTEN_FLAGS,
    OFS_GENERIC_FLAGS,
    OFS_CRITTER_FLAGS,
    OFS_CRITTER_FLAGS2,
    OFS_PC_FLAGS,
    OFS_NPC_FLAGS,
    OFS_TRAP_FLAGS,
    OFS_COUNT,
} ObjectFlagSet;

extern const char* obj_flags_fields_lookup_tbl_keys[OFS_COUNT];
extern int obj_flags_fields_lookup_tbl_values[OFS_COUNT];
extern const char** obj_flags_tbl[OFS_COUNT];
extern int obj_flags_size_tbl[OFS_COUNT];
extern const char* obj_flags_of[OF_COUNT];
extern const char* obj_flags_ofs[OSF_COUNT];
extern const char* obj_flags_owaf[OWAF_COUNT];
extern const char* obj_flags_opf[OPF_COUNT];
extern const char* obj_flags_ocof[OCOF_COUNT];
extern const char* obj_flags_oscf[OSCF_COUNT];
extern const char* obj_flags_oif[OIF_COUNT];
extern const char* obj_flags_owf[OWF_COUNT];
extern const char* obj_flags_oaf[OAF_COUNT];
extern const char* obj_flags_oarf[OARF_COUNT];
extern const char* obj_flags_ogof[OGOF_COUNT];
extern const char* obj_flags_off[OFF_COUNT];
extern const char* obj_flags_osrf[OSRF_COUNT];
extern const char* obj_flags_okrf[OKRF_COUNT];
extern const char* obj_flags_owrf[OWRF_COUNT];
extern const char* obj_flags_ogf[OGF_COUNT];
extern const char* obj_flags_ocf[OCF_COUNT];
extern const char* obj_flags_ocf2[OCF2_COUNT];
extern const char* obj_flags_opcf[OPCF_COUNT];
extern const char* obj_flags_onf[ONF_COUNT];
extern const char* obj_flags_otf[OTF_COUNT];

#endif /* ARCANUM_GAME_OBJ_FLAGS_H_ */
