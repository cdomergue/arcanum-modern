#include "game/obj_flags.h"

#include "game/obj.h"

// 0x5B9FBC
const char* obj_flags_fields_lookup_tbl_keys[OFS_COUNT] = {
    /*           OFS_FLAGS */ "obj_f_flags",
    /*     OFS_SPELL_FLAGS */ "obj_f_spell_flags",
    /*      OFS_WALL_FLAGS */ "obj_f_wall_flags",
    /*    OFS_PORTAL_FLAGS */ "obj_f_portal_flags",
    /* OFS_CONTAINER_FLAGS */ "obj_f_container_flags",
    /*   OFS_SCENERY_FLAGS */ "obj_f_scenery_flags",
    /*      OFS_ITEM_FLAGS */ "obj_f_item_flags",
    /*    OFS_WEAPON_FLAGS */ "obj_f_weapon_flags",
    /*      OFS_AMMO_FLAGS */ "obj_f_ammo_flags",
    /*     OFS_ARMOR_FLAGS */ "obj_f_armor_flags",
    /*      OFS_GOLD_FLAGS */ "obj_f_gold_flags",
    /*      OFS_FOOD_FLAGS */ "obj_f_food_flags",
    /*    OFS_SCROLL_FLAGS */ "obj_f_scroll_flags",
    /*  OFS_KEY_RING_FLAGS */ "obj_f_key_ring_flags",
    /*   OFS_WRITTEN_FLAGS */ "obj_f_written_flags",
    /*   OFS_GENERIC_FLAGS */ "obj_f_generic_flags",
    /*   OFS_CRITTER_FLAGS */ "obj_f_critter_flags",
    /*  OFS_CRITTER_FLAGS2 */ "obj_f_critter_flags2",
    /*        OFS_PC_FLAGS */ "obj_f_pc_flags",
    /*       OFS_NPC_FLAGS */ "obj_f_npc_flags",
    /*      OFS_TRAP_FLAGS */ "obj_f_trap_flags",
};

// 0x5BA010
int obj_flags_fields_lookup_tbl_values[OFS_COUNT] = {
    /*           OFS_FLAGS */ OBJ_F_FLAGS,
    /*     OFS_SPELL_FLAGS */ OBJ_F_SPELL_FLAGS,
    /*      OFS_WALL_FLAGS */ OBJ_F_WALL_FLAGS,
    /*    OFS_PORTAL_FLAGS */ OBJ_F_PORTAL_FLAGS,
    /* OFS_CONTAINER_FLAGS */ OBJ_F_CONTAINER_FLAGS,
    /*   OFS_SCENERY_FLAGS */ OBJ_F_SCENERY_FLAGS,
    /*      OFS_ITEM_FLAGS */ OBJ_F_ITEM_FLAGS,
    /*    OFS_WEAPON_FLAGS */ OBJ_F_WEAPON_FLAGS,
    /*      OFS_AMMO_FLAGS */ OBJ_F_AMMO_FLAGS,
    /*     OFS_ARMOR_FLAGS */ OBJ_F_ARMOR_FLAGS,
    /*      OFS_GOLD_FLAGS */ OBJ_F_GOLD_FLAGS,
    /*      OFS_FOOD_FLAGS */ OBJ_F_FOOD_FLAGS,
    /*    OFS_SCROLL_FLAGS */ OBJ_F_SCROLL_FLAGS,
    /*  OFS_KEY_RING_FLAGS */ OBJ_F_KEY_RING_FLAGS,
    /*   OFS_WRITTEN_FLAGS */ OBJ_F_WRITTEN_FLAGS,
    /*   OFS_GENERIC_FLAGS */ OBJ_F_GENERIC_FLAGS,
    /*   OFS_CRITTER_FLAGS */ OBJ_F_CRITTER_FLAGS,
    /*  OFS_CRITTER_FLAGS2 */ OBJ_F_CRITTER_FLAGS2,
    /*        OFS_PC_FLAGS */ OBJ_F_PC_FLAGS,
    /*       OFS_NPC_FLAGS */ OBJ_F_NPC_FLAGS,
    /*      OFS_TRAP_FLAGS */ OBJ_F_TRAP_FLAGS,
};

// 0x5BA064
const char** obj_flags_tbl[OFS_COUNT] = {
    /*           OFS_FLAGS */ obj_flags_of,
    /*     OFS_SPELL_FLAGS */ obj_flags_ofs,
    /*      OFS_WALL_FLAGS */ obj_flags_owaf,
    /*    OFS_PORTAL_FLAGS */ obj_flags_opf,
    /* OFS_CONTAINER_FLAGS */ obj_flags_ocof,
    /*   OFS_SCENERY_FLAGS */ obj_flags_oscf,
    /*      OFS_ITEM_FLAGS */ obj_flags_oif,
    /*    OFS_WEAPON_FLAGS */ obj_flags_owf,
    /*      OFS_AMMO_FLAGS */ obj_flags_oaf,
    /*     OFS_ARMOR_FLAGS */ obj_flags_oarf,
    /*      OFS_GOLD_FLAGS */ obj_flags_ogof,
    /*      OFS_FOOD_FLAGS */ obj_flags_off,
    /*    OFS_SCROLL_FLAGS */ obj_flags_osrf,
    /*  OFS_KEY_RING_FLAGS */ obj_flags_okrf,
    /*   OFS_WRITTEN_FLAGS */ obj_flags_owrf,
    /*   OFS_GENERIC_FLAGS */ obj_flags_ogf,
    /*   OFS_CRITTER_FLAGS */ obj_flags_ocf,
    /*  OFS_CRITTER_FLAGS2 */ obj_flags_ocf2,
    /*        OFS_PC_FLAGS */ obj_flags_opcf,
    /*       OFS_NPC_FLAGS */ obj_flags_onf,
    /*      OFS_TRAP_FLAGS */ obj_flags_otf,
};

// 0x5BA0B8
int obj_flags_size_tbl[OFS_COUNT] = {
    /*           OFS_FLAGS */ OF_COUNT,
    /*     OFS_SPELL_FLAGS */ OSF_COUNT,
    /*      OFS_WALL_FLAGS */ OWAF_COUNT,
    /*    OFS_PORTAL_FLAGS */ OPF_COUNT,
    /* OFS_CONTAINER_FLAGS */ OCOF_COUNT,
    /*   OFS_SCENERY_FLAGS */ OSCF_COUNT,
    /*      OFS_ITEM_FLAGS */ OIF_COUNT,
    /*    OFS_WEAPON_FLAGS */ OWF_COUNT,
    /*      OFS_AMMO_FLAGS */ OAF_COUNT,
    /*     OFS_ARMOR_FLAGS */ OARF_COUNT,
    /*      OFS_GOLD_FLAGS */ OGOF_COUNT,
    /*      OFS_FOOD_FLAGS */ OFF_COUNT,
    /*    OFS_SCROLL_FLAGS */ OSRF_COUNT,
    /*  OFS_KEY_RING_FLAGS */ OKRF_COUNT,
    /*   OFS_WRITTEN_FLAGS */ OWRF_COUNT,
    /*   OFS_GENERIC_FLAGS */ OGF_COUNT,
    /*   OFS_CRITTER_FLAGS */ OCF_COUNT,
    /*  OFS_CRITTER_FLAGS2 */ OCF2_COUNT,
    /*        OFS_PC_FLAGS */ OPCF_COUNT,
    /*       OFS_NPC_FLAGS */ ONF_COUNT,
    /*      OFS_TRAP_FLAGS */ OTF_COUNT,
};

// 0x5BA10C
const char* obj_flags_of[OF_COUNT] = {
    "OF_DESTROYED",
    "OF_OFF",
    "OF_FLAT",
    "OF_TEXT",
    "OF_SEE_THROUGH",
    "OF_SHOOT_THROUGH",
    "OF_TRANSLUCENT",
    "OF_SHRUNK",
    "OF_DONTDRAW",
    "OF_INVISIBLE",
    "OF_NO_BLOCK",
    "OF_CLICK_THROUGH",
    "OF_INVENTORY",
    "OF_DYNAMIC",
    "OF_PROVIDES_COVER",
    "OF_HAS_OVERLAYS",
    "OF_HAS_UNDERLAYS",
    "OF_WADING",
    "OF_WATER_WALKING",
    "OF_STONED",
    "OF_DONTLIGHT",
    "OF_TEXT_FLOATER",
    "OF_INVULNERABLE",
    "OF_EXTINCT",
    "OF_TRAP_PC",
    "OF_TRAP_SPOTTED",
    "OF_DISALLOW_WADING",
    "OF_MULTIPLAYER_LOCK",
    "OF_FROZEN",
    "OF_ANIMATED_DEAD",
    "OF_TELEPORTED",
};

// 0x5BA188
const char* obj_flags_ofs[OSF_COUNT] = {
    "OSF_INVISIBLE",
    "OSF_FLOATING",
    "OSF_BODY_OF_AIR",
    "OSF_BODY_OF_EARTH",
    "OSF_BODY_OF_FIRE",
    "OSF_BODY_OF_WATER",
    "OSF_DETECTING_MAGIC",
    "OSF_DETECTING_ALIGNMENT",
    "OSF_DETECTING_TRAPS",
    "OSF_DETECTING_INVISIBLE",
    "OSF_SHIELDED",
    "OSF_ANTI_MAGIC_SHELL",
    "OSF_BONDS_OF_MAGIC",
    "OSF_FULL_REFLECTION",
    "OSF_SUMMONED",
    "OSF_ILLUSION",
    "OSF_STONED",
    "OSF_POLYMORPHED",
    "OSF_MIRRORED",
    "OSF_SHRUNK",
    "OSF_PASSWALLED",
    "OSF_WATER_WALKING",
    "OSF_MAGNETIC_INVERSION",
    "OSF_CHARMED",
    "OSF_ENTANGLED",
    "OSF_SPOKEN_WITH_DEAD",
    "OSF_TEMPUS_FUGIT",
    "OSF_MIND_CONTROLLED",
    "OSF_DRUNK",
    "OSF_ENSHROUDED",
    "OSF_FAMILIAR",
    "OSF_HARDENED_HANDS",
};

// 0x5BA208
const char* obj_flags_owaf[OWAF_COUNT] = {
    "OWAF_TRANS_DISALLOW",
    "OWAF_TRANS_LEFT",
    "OWAF_TRANS_RIGHT",
    "OWAF_TRANS_ALL",
};

// 0x5BA218
const char* obj_flags_opf[OPF_COUNT] = {
    "OPF_LOCKED",
    "OPF_JAMMED",
    "OPF_MAGICALLY_HELD",
    "OPF_NEVER_LOCKED",
    "OPF_ALWAYS_LOCKED",
    "OPF_LOCKED_DAY",
    "OPF_LOCKED_NIGHT",
    "OPF_BUSTED",
    "OPF_STICKY",
};

// 0x5BA23C
const char* obj_flags_ocof[OCOF_COUNT] = {
    "OCOF_LOCKED",
    "OCOF_JAMMED",
    "OCOF_MAGICALLY_HELD",
    "OCOF_NEVER_LOCKED",
    "OCOF_ALWAYS_LOCKED",
    "OCOF_LOCKED_DAY",
    "OCOF_LOCKED_NIGHT",
    "OCOF_BUSTED",
    "OCOF_STICKY",
};

// 0x5BA260
const char* obj_flags_oscf[OSCF_COUNT] = {
    "OSCF_NO_AUTO_ANIMATE",
    "OSCF_BUSTED",
    "OSCF_NOCTURNAL",
    "OSCF_MARKS_TOWNMAP",
    "OSCF_IS_FIRE",
    "OSCF_RESPAWNABLE",
    "OSCF_SOUND_SMALL",
    "OSCF_SOUND_MEDIUM",
    "OSCF_SOUND_EXTRA_LARGE",
};

// 0x5BA284
const char* obj_flags_oif[OIF_COUNT] = {
    "OIF_IDENTIFIED",
    "OIF_WONT_SELL",
    "OIF_IS_MAGICAL",
    "OIF_TRANSFER_LIGHT",
    "OIF_NO_DISPLAY",
    "OIF_NO_DROP",
    "OIF_HEXED",
    "OIF_CAN_USE_BOX",
    "OIF_NEEDS_TARGET",
    "OIF_LIGHT_SMALL",
    "OIF_LIGHT_MEDIUM",
    "OIF_LIGHT_LARGE",
    "OIF_LIGHT_XLARGE",
    "OIF_PERSISTENT",
    "OIF_MT_TRIGGERED",
    "OIF_STOLEN",
    "OIF_USE_IS_THROW",
    "OIF_NO_DECAY",
    "OIF_UBER",
    "OIF_NO_NPC_PICKUP",
    "OIF_NO_RANGED_USE",
    "OIF_VALID_AI_ACTION",
    "OIF_MP_INSERTED",
};

// 0x5BA2E0
const char* obj_flags_owf[OWF_COUNT] = {
    "OWF_LOUD",
    "OWF_SILENT",
    "OWF_TWO_HANDED",
    "OWF_HAND_COUNT_FIXED",
    "OWF_THROWABLE",
    "OWF_TRANS_PROJECTILE",
    "OWF_BOOMERANGS",
    "OWF_IGNORE_RESISTANCE",
    "OWF_DAMAGE_ARMOR",
    "OWF_DEFAULT_THROWS",
};

// 0x5BA308
const char* obj_flags_oaf[OAF_COUNT] = {
    "OAF_NONE",
};

// 0x5BA30C
const char* obj_flags_oarf[OARF_COUNT] = {
    "OARF_SIZE_SMALL",
    "OARF_SIZE_MEDIUM",
    "OARF_SIZE_LARGE",
    "OARF_MALE_ONLY",
    "OARF_FEMALE_ONLY",
};

// 0x5BA320
const char* obj_flags_ogof[OGOF_COUNT] = {
    "OGOF_NONE",
};

// 0x5BA324
const char* obj_flags_off[OFF_COUNT] = {
    "OFF_NONE",
};

// 0x5BA328
const char* obj_flags_osrf[OSRF_COUNT] = {
    "OSRF_NONE",
};

// 0x5BA32C
const char* obj_flags_okrf[OKRF_COUNT] = {
    "OKRF_PRIMARY_RING",
};

// 0x5BA330
const char* obj_flags_owrf[OWRF_COUNT] = {
    "OWRF_NONE",
};

// 0x5BA334
const char* obj_flags_ogf[OGF_COUNT] = {
    "OGF_USES_TORCH_SHIELD_LOCATION",
    "OGF_IS_LOCKPICK",
    "OGF_IS_TRAP_DEVICE",
    NULL,
    NULL,
};

// 0x5BA348
const char* obj_flags_ocf[OCF_COUNT] = {
    "OCF_IS_CONCEALED",
    "OCF_MOVING_SILENTLY",
    "OCF_UNDEAD",
    "OCF_ANIMAL",
    "OCF_FLEEING",
    "OCF_STUNNED",
    "OCF_PARALYZED",
    "OCF_BLINDED",
    "OCF_CRIPPLED_ARMS_ONE",
    "OCF_CRIPPLED_ARMS_BOTH",
    "OCF_CRIPPLED_LEGS_BOTH",
    "OCF_UNUSED",
    "OCF_SLEEPING",
    "OCF_MUTE",
    "OCF_SURRENDERED",
    "OCF_MONSTER",
    "OCF_SPELL_FLEE",
    "OCF_ENCOUNTER",
    "OCF_COMBAT_MODE_ACTIVE",
    "OCF_LIGHT_SMALL",
    "OCF_LIGHT_MEDIUM",
    "OCF_LIGHT_LARGE",
    "OCF_LIGHT_XLARGE",
    "OCF_UNREVIVIFIABLE",
    "OCF_UNRESSURECTABLE",
    "OCF_DEMON",
    "OCF_FATIGUE_IMMUNE",
    "OCF_NO_FLEE",
    "OCF_NON_LETHAL_COMBAT",
    "OCF_MECHANICAL",
    "OCF_ANIMAL_ENSHROUD",
    "OCF_FATIGUE_LIMITING",
};

// 0x5BA3C8
const char* obj_flags_ocf2[OCF2_COUNT] = {
    "OCF2_ITEM_STOLEN",
    "OCF2_AUTO_ANIMATES",
    "OCF2_USING_BOOMERANG",
    "OCF2_FATIGUE_DRAINING",
    "OCF2_SLOW_PARTY",
    "OCF2_COMBAT_TOGGLE_FX",
    "OCF2_NO_DECAY",
    "OCF2_NO_PICKPOCKET",
    "OCF2_NO_BLOOD_SPLOTCHES",
    "OCF2_NIGH_INVULNERABLE",
    "OCF2_ELEMENTAL",
    "OCF2_DARK_SIGHT",
    "OCF2_NO_SLIP",
    "OCF2_NO_DISINTEGRATE",
    "OCF2_REACTION_0",
    "OCF2_REACTION_1",
    "OCF2_REACTION_2",
    "OCF2_REACTION_3",
    "OCF2_REACTION_4",
    "OCF2_REACTION_5",
    "OCF2_REACTION_6",
    "OCF2_TARGET_LOCK",
    "OCF2_PERMA_POLYMORPH",
    "OCF2_SAFE_OFF",
    "OCF2_CHECK_REACTION_BAD",
    "OCF2_CHECK_ALIGN_GOOD",
    "OCF2_CHECK_ALIGN_BAD",
};

// 0x5BA434
const char* obj_flags_opcf[OPCF_COUNT] = {
    "OPCF_unused_1",
    "OPCF_unused_2",
    "OPCF_USE_ALT_DATA",
    "OPCF_unused_4",
    "OPCF_unused_5",
    "OPCF_FOLLOWER_SKILLS_ON",
};

// 0x5BA44C
const char* obj_flags_onf[ONF_COUNT] = {
    "ONF_FIGHTING",
    "ONF_WAYPOINTS_DAY",
    "ONF_WAYPOINTS_NIGHT",
    "ONF_AI_WAIT_HERE",
    "ONF_AI_SPREAD_OUT",
    "ONF_JILTED",
    "ONF_CHECK_WIELD",
    "ONF_CHECK_WEAPON",
    "ONF_KOS",
    "ONF_WAYPOINTS_BED",
    "ONF_FORCED_FOLLOWER",
    "ONF_KOS_OVERRIDE",
    "ONF_WANDERS",
    "ONF_WANDERS_IN_DARK",
    "ONF_FENCE",
    "ONF_FAMILIAR",
    "ONF_CHECK_LEADER",
    "ONF_ALOOF",
    "ONF_CAST_HIGHEST",
    "ONF_GENERATOR",
    "ONF_GENERATED",
    "ONF_GENERATOR_RATE1",
    "ONF_GENERATOR_RATE2",
    "ONF_GENERATOR_RATE3",
    "ONF_DEMAINTAIN_SPELLS",
    "ONF_LOOK_FOR_WEAPON",
    "ONF_LOOK_FOR_ARMOR",
    "ONF_LOOK_FOR_AMMO",
    "ONF_BACKING_OFF",
    "ONF_NO_ATTACK",
    NULL,
};

// 0x5BA4C8
const char* obj_flags_otf[OTF_COUNT] = {
    "OTF_BUSTED",
};
