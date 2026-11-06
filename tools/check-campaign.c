/* Full-engine dialogue regression, restricted to marked disposable games.
 * Uses the installed dialogue and native quest/inventory/save implementations.
 * A follower is the stand-in speaker: this does not simulate the entire quest.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#define chdir _chdir
#else
#include <unistd.h>
#endif
#include <tig/tig.h>
#include "game/damage_type.h"
#include "game/dialog.h"
#include "game/descriptions.h"
#include "game/effect.h"
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/item.h"
#include "game/name.h"
#include "game/map.h"
#include "game/sector.h"
#include "game/area.h"
#include "game/location.h"
#include "game/anim.h"
#include "game/magictech.h"
#include "game/critter.h"
#include "game/fate.h"
#include "ui/inven_ui.h"
#include "ui/follower_ui.h"
#include "game/townmap.h"
#include "ui/wmap_ui.h"
#include "game/modern_world_updates.h"
#include "game/object.h"
#include "game/player.h"
#include "game/proto.h"
#include "game/quest.h"
#include "game/reputation.h"
#include "game/script.h"
#include "game/script_name.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/stat.h"
#include "game/teleport.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/mainmenu_ui.h"
#ifndef CAMPAIGN_PURE_CE
#include "ui/modern_ui.h"
#endif

#define REQUIRE(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)

static int open_dialog(DialogState* state)
{
    memset(state, 0, sizeof(*state));
    state->pc_obj = player_get_local_pc_obj();
    ObjectList followers;
    object_list_followers(state->pc_obj, &followers);
    state->npc_obj = followers.head ? followers.head->obj : OBJ_HANDLE_NULL;
    object_list_destroy(&followers);
    REQUIRE(state->npc_obj != OBJ_HANDLE_NULL);
    state->script_num = 1565;
    char path[TIG_MAX_PATH];
    REQUIRE(script_name_build_dlg_name(state->script_num, path, sizeof(path)));
    REQUIRE(dialog_load(path, &state->dlg));
    return 0;
}

static void visit(DialogState* state, int line)
{
    state->num = line;
    sub_413A30(state, true);
}

static int reward(const char* input, int line, int amount, const char* output)
{
    REQUIRE(gamelib_load(input));
    DialogState state;
    REQUIRE(open_dialog(&state) == 0);
    REQUIRE(quest_state_get(state.pc_obj, 1098) < QUEST_STATE_COMPLETED);
    stat_base_set(state.pc_obj, STAT_CHARISMA, 20);
    basic_skill_points_set(state.pc_obj, BASIC_SKILL_PERSUATION, 5);
    REQUIRE(basic_skill_training_set(state.pc_obj, BASIC_SKILL_PERSUATION, TRAINING_APPRENTICE) == TRAINING_APPRENTICE);
    REQUIRE(basic_skill_training_set(state.pc_obj, BASIC_SKILL_PERSUATION, TRAINING_EXPERT) == TRAINING_EXPERT);
    quest_state_set(state.pc_obj, 1098, QUEST_STATE_ACHIEVED, state.npc_obj);
    int before = item_gold_get(state.pc_obj);
    visit(&state, line);
    REQUIRE(item_gold_get(state.pc_obj) == before + amount);
    REQUIRE(quest_state_get(state.pc_obj, 1098) == QUEST_STATE_COMPLETED);
    visit(&state, line); /* No later player response/local exit flag. */
    REQUIRE(item_gold_get(state.pc_obj) == before + amount);
    if (line == 73) REQUIRE(reputation_has(state.pc_obj, 1044));
    dialog_unload(state.dlg);
    REQUIRE(gamelib_save(output, "Campaign reward compatibility fixture"));
    REQUIRE(gamelib_load(output));
    REQUIRE(open_dialog(&state) == 0);
    REQUIRE(quest_state_get(state.pc_obj, 1098) == QUEST_STATE_COMPLETED);
    visit(&state, line);
    REQUIRE(item_gold_get(state.pc_obj) == before + amount);
    visit(&state, 80); /* The training action must remain available. */
    REQUIRE(basic_skill_training_get(state.pc_obj, 11) == 3);
    REQUIRE(reputation_has(state.pc_obj, 1018));
    REQUIRE(item_gold_get(state.pc_obj) == before + amount);
    dialog_unload(state.dlg);
    REQUIRE(gamelib_save(output, "Campaign reward and training compatibility fixture"));
    printf("PASS: line %d pays %d once; native completed state survives reload; training and reputation retained\n", line, amount);
    return 0;
}

static int lower_reward(const char* input)
{
    REQUIRE(gamelib_load(input));
    DialogState state;
    REQUIRE(open_dialog(&state) == 0);
    int before = item_gold_get(state.pc_obj);
    visit(&state, 35);
    REQUIRE(quest_state_get(state.pc_obj, 1098) == QUEST_STATE_COMPLETED);
    /* This is a PC response, so use the native option-action entry point. */
    state.field_17E8 = 0;
    state.actions[0] = "$$3000, lf3 1";
    state.field_17F0[0] = 1;
    sub_413130(&state, 0);
    REQUIRE(item_gold_get(state.pc_obj) == before + 3000);
    dialog_unload(state.dlg);
    puts("PASS: completed quest does not suppress the separate 3000-gold player response");
    return 0;
}

typedef struct Snapshot {
    int stats[16], gold, quest, training, items, daggers, xp;
    bool diplomat, master;
    int64_t location;
} Snapshot;

static Snapshot snapshot(void)
{
    Snapshot result = {0};
    int64_t pc = player_get_local_pc_obj();
    for (int i = 0; i < 16; i++) result.stats[i] = stat_level_get(pc, i);
    result.gold = item_gold_get(pc);
    result.quest = quest_state_get(pc, 1098);
    result.training = basic_skill_training_get(pc, BASIC_SKILL_PERSUATION);
    result.xp = stat_base_get(pc, STAT_EXPERIENCE_POINTS);
    result.diplomat = reputation_has(pc, 1044);
    result.master = reputation_has(pc, 1018);
    result.location = obj_field_int64_get(pc, OBJ_F_LOCATION);
    result.items = obj_field_int32_get(pc, OBJ_F_CRITTER_INVENTORY_NUM);
    for (int i = 0; i < result.items; i++) {
        int64_t item = obj_arrayfield_handle_get(pc, OBJ_F_CRITTER_INVENTORY_LIST_IDX, i);
        int64_t proto = obj_field_handle_get(item, OBJ_F_PROTOTYPE_HANDLE);
        if (objid_is_equal(obj_get_id(proto), obj_get_id(sub_4685A0(BP_DAGGER)))) result.daggers++;
    }
    return result;
}

static int roundtrip(const char* input)
{
    REQUIRE(gamelib_load(input));
    Snapshot before = snapshot();
    printf("READ: %s ST=%d speed=%d gold=%d items=%d daggers=%d\n", input,
        before.stats[STAT_STRENGTH], before.stats[STAT_SPEED], before.gold, before.items, before.daggers);
    REQUIRE(before.stats[STAT_STRENGTH] == 11 && before.stats[STAT_SPEED] == 10);
    REQUIRE(before.daggers == 2);
    REQUIRE(gamelib_save("SlotCE00", "Native engine roundtrip fixture"));
    REQUIRE(gamelib_load("SlotCE00"));
    Snapshot after = snapshot();
    REQUIRE(memcmp(before.stats, after.stats, sizeof(before.stats)) == 0);
    REQUIRE(before.gold == after.gold && before.quest == after.quest && before.training == after.training);
    REQUIRE(before.items == after.items && before.daggers == after.daggers && before.location == after.location);
    REQUIRE(before.xp == after.xp && before.diplomat == after.diplomat && before.master == after.master);
    printf("PASS: %s load/save/reload: ST=%d speed=%d followers=%d gold=%d daggers=%d quest=%d training=%d\n",
        input, after.stats[STAT_STRENGTH], after.stats[STAT_SPEED], after.stats[STAT_MAX_FOLLOWERS],
        after.gold, after.daggers, after.quest, after.training);
    return 0;
}

static int create_fixture(void)
{
    /* A fresh native fixture, never derived from an installation's saves. */
    PlayerCreateInfo info;
    player_create_info_init(&info);
    info.loc = obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION);
    info.basic_prototype = 16066;
    REQUIRE(player_obj_create_player(&info));
    obj_field_string_set(info.obj, OBJ_F_PC_PLAYER_NAME, "CE compatibility fixture");
    stat_base_set(info.obj, STAT_STRENGTH, 11);
    effect_add(info.obj, 298, EFFECT_CAUSE_BACKGROUND);
    effect_add(info.obj, 298, EFFECT_CAUSE_BACKGROUND);
    effect_add(info.obj, 307, EFFECT_CAUSE_BACKGROUND);
    REQUIRE(item_gold_transfer(OBJ_HANDLE_NULL, info.obj, 12345, OBJ_HANDLE_NULL));
    for (int i = 0; i < 2; i++) {
        int64_t dagger;
        REQUIRE(object_create(sub_4685A0(BP_DAGGER), info.loc, &dagger));
        REQUIRE(item_transfer(dagger, info.obj));
    }
    int map = map_by_type(MAP_TYPE_START_MAP);
    int64_t x, y;
    REQUIRE(map > 0 && map_get_starting_location(map, &x, &y));
    TeleportData move = {0};
    move.obj = info.obj;
    move.map = map;
    move.loc = location_make(x, y);
    REQUIRE(teleport_do(&move));
    teleport_ping(0);
    object_flags_unset(player_get_local_pc_obj(), OF_OFF);
    REQUIRE(gamelib_save("SlotBASE", "Fresh native base-game fixture"));
    return roundtrip("SlotBASE");
}

static int inspect_prototypes(void)
{
    const int descriptions[] = {BP_AIR_ELEMENTAL, BP_EARTH_ELEMENTAL,
        BP_FIRE_ELEMENTAL, BP_WATER_ELEMENTAL, BP_EVIL_TEMPEST,
        BP_MECHANIZED_ARACHNID_1, BP_MECHANIZED_ARACHNID_2};
    for (unsigned i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); i++) {
        int64_t obj = sub_4685A0(descriptions[i]);
        REQUIRE(obj != OBJ_HANDLE_NULL);
        printf("PROTOTYPE %d flags=%08x flags2=%08x faction=%d spells=",
            obj_get_id(obj).d.a, obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS),
            obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2), obj_field_int32_get(obj, OBJ_F_NPC_FACTION));
        for (int j = 0; j < SPELL_COUNT; j++)
            if (spell_is_known(obj, j)) printf("%d,", j);
        printf(" damage=");
        for (int j = 0; j < DAMAGE_TYPE_COUNT * 2; j++)
            printf("%d,", obj_arrayfield_int32_get(obj, OBJ_F_NPC_DAMAGE_IDX, j));
        puts("");
    }
    return 0;
}

static int inspect_actors(void)
{
    FILE* list = fopen("actor-audit.txt", "r");
    REQUIRE(list != NULL);
    char path[TIG_MAX_PATH];
    while (fgets(path, sizeof(path), list)) {
        path[strcspn(path, "\r\n")] = '\0';
        REQUIRE(!strncmp(path, "maps/", 5) && !strstr(path, ".."));
        for (char* cursor = path; *cursor; cursor++)
            if (*cursor == '/') *cursor = '\\';
        TigFile* file = tig_file_fopen(path, "rb");
        REQUIRE(file != NULL);
        int64_t obj;
        REQUIRE(obj_read(file, &obj));
        tig_file_fclose(file);
        if (obj_field_int32_get(obj, OBJ_F_TYPE) == OBJ_TYPE_NPC) {
            char id[OID_STR_LEN];
            objid_id_to_str(id, obj_get_id(obj));
            printf("ACTOR %s id=%s proto=%d flags=%08x flags2=%08x npcflags=%08x scripts=", path, id,
                obj_get_id(obj_field_handle_get(obj, OBJ_F_PROTOTYPE_HANDLE)).d.a,
                obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS), obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2),
                obj_field_int32_get(obj, OBJ_F_NPC_FLAGS));
            for (int i = 0; i <= SAP_CRITICAL_MISS; i++) {
                Script script;
                obj_arrayfield_script_get(obj, OBJ_F_SCRIPTS_IDX, i, &script);
                if (script.num) printf("%d:%d,", i, script.num);
            }
            puts("");
        }
        obj_deallocate(obj);
    }
    fclose(list);
    return 0;
}

static int check_arachnid(unsigned expected_flags)
{
    ObjectID expected;
    REQUIRE(objid_id_from_str(&expected, "G_47F7C5DE_8015_D411_8F1D_00E02920220C"));
    int64_t obj;
    int iter;
    if (obj_inst_first(&obj, &iter)) {
        do {
            if (objid_is_equal(obj_get_id(obj), expected)) {
                unsigned flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
                printf("ARACHNID: flags=%08x map=%d\n", flags, map_current_map());
                REQUIRE(flags == expected_flags);
                REQUIRE(obj_get_id(obj_field_handle_get(obj, OBJ_F_PROTOTYPE_HANDLE)).d.a == 28343);
                return 0;
            }
        } while (obj_inst_next(&obj, &iter));
    }
    REQUIRE(!"Native Tarant sewer arachnid was not loaded");
    return 1;
}

static int arachnid_roundtrip(const char* input, bool original)
{
    REQUIRE(gamelib_load(input));
    int map = map_list_info_find("Tarant Sewers-01") + 1;
    REQUIRE(map > 0);
    if (map_current_map() != map) {
        int64_t x, y;
        REQUIRE(map_get_starting_location(map, &x, &y));
        TeleportData move = {0};
        move.obj = player_get_local_pc_obj();
        move.map = map;
        move.loc = location_make(x, y);
        REQUIRE(teleport_do(&move));
        teleport_ping(0);
    }
    REQUIRE(map_current_map() == map);
    unsigned expected_flags = original ? 0x2003u : 0x20002003u;
    const char* output = original ? "SlotOLDM" : "SlotSPDR";
    REQUIRE(check_arachnid(expected_flags) == 0);
    REQUIRE(gamelib_save(output, "Native mechanical arachnid compatibility fixture"));
    REQUIRE(gamelib_load(output));
    REQUIRE(check_arachnid(expected_flags) == 0);
    printf("PASS: %s native sewer instance flags survive save/reload\n", original ? "original" : "corrected");
    return 0;
}

static uint64_t save_archive_hash(const char* path)
{
    FILE* file = fopen(path, "rb");
    if (!file) return 0;
    uint64_t hash = UINT64_C(14695981039346656037);
    int ch;
    while ((ch = fgetc(file)) != EOF) hash = (hash ^ (unsigned char)ch) * UINT64_C(1099511628211);
    bool ok = !ferror(file);
    fclose(file);
    return ok ? hash : 0;
}

static int save_retry_roundtrip(const char* input)
{
    REQUIRE(gamelib_load(input));
    REQUIRE(modern_world_updates_finish_load());
    Snapshot before = snapshot();
    REQUIRE(gamelib_save("SlotSR01", "Save retry baseline"));
    uint64_t archive = save_archive_hash("modules/Arcanum/Save/SlotSR01.tfaf");
    uint64_t index = save_archive_hash("modules/Arcanum/Save/SlotSR01.tfai");
    REQUIRE(archive && index);
    int64_t id = sector_id_from_loc(before.location);
    Sector* sector;
    REQUIRE(sector_lock(id, &sector));
    REQUIRE(!sector_flush(1));
    REQUIRE(sector_save_failed());
    // Retrying while the cause persists must retain the old published save.
    for (int attempt = 0; attempt < 2; attempt++) {
        REQUIRE(!gamelib_save("SlotSR01", "Must not replace the baseline"));
        REQUIRE(sector_save_failed());
        REQUIRE(save_archive_hash("modules/Arcanum/Save/SlotSR01.tfaf") == archive);
        REQUIRE(save_archive_hash("modules/Arcanum/Save/SlotSR01.tfai") == index);
    }
    REQUIRE(sector_unlock(id));
    // Only the production save entry point may clear the recovered failure.
    REQUIRE(gamelib_save("SlotSR02", "Save after recovered sector failure"));
    REQUIRE(!sector_save_failed());
    REQUIRE(sector_lock(id, &sector));
    REQUIRE(!sector_flush(1));
    REQUIRE(!map_flush(1));
    REQUIRE(sector_save_failed());
    REQUIRE(sector_unlock(id));
    REQUIRE(map_flush(1));
    REQUIRE(!sector_save_failed());
    REQUIRE(gamelib_load("SlotSR02"));
    Snapshot after = snapshot();
    REQUIRE(memcmp(&before, &after, sizeof(before)) == 0);
    printf("PASS: persistent sector failure preserves published save; save and map flush recover after unlock; reload preserves player state\n");
    return 0;
}

static int hardin_route_roundtrip(const char* input)
{
    REQUIRE(gamelib_load(input));
    REQUIRE(modern_world_updates_finish_load());
    int64_t pc = player_get_local_pc_obj();
    REQUIRE(area_set_known(pc, 17));
    int64_t origin = obj_field_int64_get(pc, OBJ_F_LOCATION);
    bool outside = false;
    // Start outside town bounds, where the native UI permits world travel.
    for (int distance = 64; distance <= 4096; distance += 64) {
        int64_t loc = location_make(LOCATION_GET_X(origin) + distance, LOCATION_GET_Y(origin));
        int64_t sector = sector_id_from_loc(loc);
        if (townmap_get(sector) == TOWNMAP_NONE && !sector_is_blocked(sector)) {
            TeleportData move = {0};
            move.obj = pc; move.loc = loc; move.map = map_current_map();
            REQUIRE(teleport_do(&move)); teleport_ping(0);
            outside = true; break;
        }
    }
    REQUIRE(outside);
    wmap_ui_open();
    ModernTravelMap travel;
    REQUIRE(modern_map_travel_state(&travel));
    ModernMapLabel labels[200];
    int count = modern_map_travel_labels(labels, 200), target = -1;
    for (int i = 0; i < count; i++) if (!strcmp(labels[i].text, area_get_name(17))) target = i;
    REQUIRE(target >= 0);
    int64_t destination = area_get_location(17);
    REQUIRE(sector_is_blocked(sector_id_from_loc(destination)));
    int x = labels[target].x, y = labels[target].y;
    modern_map_travel_mouse(x, y, TIG_MESSAGE_MOUSE_MOVE);
    modern_map_travel_mouse(x, y, TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN);
    modern_map_travel_mouse(x, y, TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP);
    modern_map_action(3);
    ModernMap state;
    REQUIRE(modern_map_state(&state) && state.moving);
    int ticks = 0;
    while (wmap_ui_is_created() && ticks++ < 5000) {
        wmap_ui_bkg_process_callback(NULL);
        teleport_ping(0);
    }
    REQUIRE(ticks < 5000 && !wmap_ui_is_created());
    REQUIRE(obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION) == destination);
    REQUIRE(sector_is_blocked(sector_id_from_loc(destination)));
    REQUIRE(!sector_save_failed());
    REQUIRE(gamelib_save("SlotHR01", "Hardin marker arrival regression"));
    REQUIRE(gamelib_load("SlotHR01"));
    REQUIRE(obj_field_int64_get(player_get_local_pc_obj(), OBJ_F_LOCATION) == destination);
    printf("PASS: exact Hardin marker click travels and reloads at the destination without unblocking the pass\n");
    return 0;
}

static int check_ce_shield(const char* input)
{
    const int spells[] = {-1, SPELL_SHIELD_OF_PROTECTION, SPELL_BODY_OF_FIRE};
    const unsigned int expected[] = {0, OSF_SHIELDED, OSF_BODY_OF_FIRE};
    for (int scenario = 0; scenario < 3; scenario++) {
        REQUIRE(gamelib_load(input));
        int64_t pc = player_get_local_pc_obj();
        int64_t loc = obj_field_int64_get(pc, OBJ_F_LOCATION);
        int64_t attacker, target;
        REQUIRE(object_create(sub_4685A0(BP_WOLF), loc, &attacker));
        REQUIRE(object_create(sub_4685A0(BP_WOLF), loc, &target));
        object_hp_pts_set(attacker, 1000);
        object_hp_pts_set(target, 1000);
        REQUIRE(obj_field_int32_get(target, OBJ_F_SPELL_FLAGS) == 0);
        REQUIRE(combat_critter_weapon(attacker) == OBJ_HANDLE_NULL);
        if (scenario) {
            stat_base_set(pc, STAT_INTELLIGENCE, 20);
            stat_base_set(pc, STAT_WILLPOWER, 20);
            MagicTechInvocation invocation;
            magictech_invocation_init(&invocation, pc, spells[scenario]);
            sub_4440E0(target, &invocation.target_obj);
            invocation.target_loc = loc;
            // UNRESISTABLE also ends maintained spells in the native lifecycle.
            // Keep the normal resistance/lifecycle path on these fresh wolves.
            invocation.flags |= MAGICTECH_INVOCATION_FREE;
            magictech_invocation_run(&invocation);
            // Execute the queued spell's native BEGIN action without a UI clock.
            // This probe checks effects/hits, not casting animation scheduling.
            int spell_id = -1;
            for (int index = 0; index < 512; index++) {
                if ((magictech_run_info[index].flags & MAGICTECH_RUN_ACTIVE)
                    && magictech_run_info[index].spell == spells[scenario]
                    && magictech_run_info[index].target_obj.obj == target) {
                    spell_id = magictech_run_info[index].id;
                    break;
                }
            }
            printf("CE_SHIELD queued_spell=%d id=%d\n", spells[scenario], spell_id);
            REQUIRE(spell_id >= 0);
            sub_456F70(spell_id);
            REQUIRE((obj_field_int32_get(target, OBJ_F_SPELL_FLAGS) & expected[scenario]) != 0);
        }
        int shield_index = -1;
        int shield_spell = sub_459170(target, OSF_SHIELDED, &shield_index)
            ? magictech_run_info[shield_index].spell : -1;
        int before_attacker = object_hp_current(attacker);
        int before_target = object_hp_current(target);
        for (int hit = 0; hit < 12; hit++) {
            CombatContext combat;
            sub_4B2210(attacker, target, &combat);
            // Production forced-hit path: isolate retaliation from critical misses.
            combat.flags |= 0x100;
            combat.dam[DAMAGE_TYPE_NORMAL] = 10;
            sub_4B3170(&combat);
            REQUIRE(combat.target_obj == target);
            REQUIRE((combat.flags & CF_HIT) != 0);
        }
        int lost_attacker = before_attacker - object_hp_current(attacker);
        int lost_target = before_target - object_hp_current(target);
        printf("CE_SHIELD spell=%d resolved_shield=%d hits=12 attacker_hp_loss=%d target_hp_loss=%d target_flags=%x\n",
            spells[scenario], shield_spell, lost_attacker, lost_target,
            (unsigned int)obj_field_int32_get(target, OBJ_F_SPELL_FLAGS));
        REQUIRE(lost_target > 0);
        if (scenario == 1) REQUIRE(shield_spell == SPELL_SHIELD_OF_PROTECTION);
        REQUIRE(scenario == 2 ? lost_attacker > 0 : lost_attacker == 0);
    }
    // Separate critical-miss control: native fate forces retargeting to attacker.
    REQUIRE(gamelib_load(input));
    int64_t pc = player_get_local_pc_obj(), attacker;
    REQUIRE(object_create(sub_4685A0(BP_WOLF), obj_field_int64_get(pc, OBJ_F_LOCATION), &attacker));
    object_hp_pts_set(attacker, 1000);
    stat_base_set(pc, STAT_FATE_POINTS, 1);
    REQUIRE(fate_activate(pc, FATE_CRIT_MISS));
    int attacker_hp = object_hp_current(attacker), target_hp = object_hp_current(pc);
    CombatContext critical;
    sub_4B2210(attacker, pc, &critical);
    critical.flags |= 0x100;
    critical.dam[DAMAGE_TYPE_NORMAL] = 10;
    sub_4B3170(&critical);
    REQUIRE(critical.target_obj == attacker && (critical.flags & 0x8));
    REQUIRE(object_hp_current(attacker) < attacker_hp);
    REQUIRE(object_hp_current(pc) == target_hp);
    printf("CE_FUMBLE attacker_hp_loss=%d intended_target_hp_loss=%d flags=%x\n",
        attacker_hp - object_hp_current(attacker), target_hp - object_hp_current(pc), critical.flags);
    puts("PASS: protection has no retaliation; fire and critical-miss controls lose attacker HP");
    return 0;
}

static int check_ce_portrait_animation(const char* input)
{
    REQUIRE(gamelib_load(input));
    int64_t pc = player_get_local_pc_obj();
    ObjectList followers;
    object_list_followers(pc, &followers);
    int64_t npc = followers.head ? followers.head->obj : OBJ_HANDLE_NULL;
    object_list_destroy(&followers);
    REQUIRE(npc != OBJ_HANDLE_NULL);
    AnimID before, after;
    REQUIRE(sub_423300(npc, &before));
    AnimRunInfo* run;
    REQUIRE(anim_id_to_run_info(&before, &run));
    int goal = run->goals[run->current_goal].type;
    unsigned int flags = run->flags;
    unsigned int aid = obj_field_int32_get(npc, OBJ_F_CURRENT_AID);
    for (int i = 0; i < 60; i++) REQUIRE(inven_ui_can_open_inven_preview(pc, npc));
    modern_follower_log_menu(npc);
    REQUIRE(sub_423300(npc, &after));
    REQUIRE(anim_id_is_equal(&before, &after));
    REQUIRE(anim_id_to_run_info(&after, &run));
    REQUIRE(run->flags == flags && run->goals[run->current_goal].type == goal);
    REQUIRE((unsigned int)obj_field_int32_get(npc, OBJ_F_CURRENT_AID) == aid);
    printf("CE_PORTRAIT preview_calls=60 preserved_slot=%d goal=%d aid=%08x\n", before.slot_num, goal, aid);
    // The execution path deliberately retains its ability to stop the actor.
    REQUIRE(inven_ui_can_open_inven(pc, npc));
    bool active = sub_423300(npc, &after);
    printf("CE_PORTRAIT execute_active=%d (may stop interruptible goal)\n", active);
    REQUIRE(!active || !anim_id_is_equal(&before, &after));
    puts("PASS: menu preview preserves animation; execution still interrupts");
    return 0;
}

static int inspect_ce_fixture(const char* input)
{
    REQUIRE(gamelib_load(input));
    int64_t pc = player_get_local_pc_obj();
    int64_t loc = obj_field_int64_get(pc, OBJ_F_LOCATION);
    printf("CE_FIXTURE slot=%s map=%d loc=%llu area=%d townmap=%d spell_flags=%x\n",
        input, map_current_map(), (unsigned long long)loc, area_of_object(pc),
        townmap_get(sector_id_from_loc(loc)), obj_field_int32_get(pc, OBJ_F_SPELL_FLAGS));
    for (int area = 1; area < area_count(); area++) {
        int64_t origin = area_get_location(area);
        int64_t x = LOCATION_GET_X(origin), y = LOCATION_GET_Y(origin);
        printf("CE_AREA id=%d x=%lld y=%lld townmap=%d name=%s\n", area,
            (long long)x, (long long)y, townmap_get(sector_id_from_loc(origin)), area_get_name(area));
    }
    for (int map = 1; map < townmap_count(); map++) {
        TownMapInfo info;
        if (townmap_info(map, &info)) printf("CE_TOWNMAP id=%d name=%s map=%d origin=%llu w=%d h=%d cols=%d rows=%d\n",
            map, townmap_name(map), info.map, (unsigned long long)info.loc,
            info.width, info.height, info.num_hor_tiles, info.num_vert_tiles);
    }
    puts("PASS: CE fixture loaded and location metadata inspected");
    return 0;
}

static int check_ce_isle_area(const char* input)
{
    REQUIRE(gamelib_load(input));
    REQUIRE(map_current_map() == map_by_type(MAP_TYPE_START_MAP));
    int64_t pc = player_get_local_pc_obj();
    int64_t original = obj_field_int64_get(pc, OBJ_F_LOCATION);
    int64_t origin = area_get_location(19);
    printf("CE_ISLE origin_x=%lld origin_y=%lld townmap=%d\n",
        (long long)LOCATION_GET_X(origin), (long long)LOCATION_GET_Y(origin),
        townmap_get(sector_id_from_loc(origin)));
    // This test only changes the in-memory location field, restores it before
    // returning, and never serializes the diagnostic state.
    obj_field_int64_set(pc, OBJ_F_LOCATION, origin);
    int detected = area_of_object(pc);
    obj_field_int64_set(pc, OBJ_F_LOCATION, original);
    printf("CE_ISLE area_of_object=%d expected=19\n", detected);
    REQUIRE(detected == 19);
    puts("PASS: installed Isle of Despair origin resolves to dialogue area 19");
    return 0;
}

int main(int argc, char** argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    if ((argc != 3 && argc != 4) || chdir(argv[1])) return 2;
    FILE* marker = fopen(".renaissance-campaign-test", "rb");
    if (!marker) return 2;
    bool marked = fgetc(marker) == '1' && fgetc(marker) == '\n' && fgetc(marker) == EOF;
    fclose(marker);
    if (!marked || (strcmp(argv[2], "--create") && strcmp(argv[2], "--prototypes") && strcmp(argv[2], "--actors") &&
        (strlen(argv[2]) != 8 || strncmp(argv[2], "Slot", 4)))) return 2;
    if (argc == 4 && strcmp(argv[3], "--roundtrip") && strcmp(argv[3], "--arachnid")
        && strcmp(argv[3], "--arachnid-original") && strcmp(argv[3], "--save-retry") && strcmp(argv[3], "--hardin-route") && strcmp(argv[3], "--ce-inspect") && strcmp(argv[3], "--ce-isle-area") && strcmp(argv[3], "--ce-portrait") && strcmp(argv[3], "--ce-shield")) return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER, "software", SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init = {0};
    init.width = highres_config_get()->width;
    init.height = highres_config_get()->height;
    init.bpp = 32;
    init.texture_width = init.texture_height = 2048;
    init.flags = TIG_INITIALIZE_WINDOWED | TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver = name_resolve_path;
    init.art_id_reset_func = name_normalize_aid;
    init.sound_file_path_resolver = gsound_resolve_path;
    REQUIRE(tig_init(&init) == TIG_OK);
    intgame_set_iso_window_width(init.width);
    intgame_set_iso_window_height(init.height);
    GameInitInfo game = {0};
    game.draw_func = iso_redraw;
    game.invalidate_rect_func = iso_invalidate_rect;
    REQUIRE(intgame_create_iso_window(&game.iso_window_handle));
    REQUIRE(gamelib_init(&game));
    REQUIRE(gameuilib_init(&game));
    REQUIRE(gamelib_mod_load("Arcanum"));
    REQUIRE(gameuilib_mod_load());
#ifndef CAMPAIGN_PURE_CE
    gamelib_splash_clear();
    REQUIRE(modern_ui_init());
#endif
    int result;
    if (!strcmp(argv[2], "--actors")) result = inspect_actors();
    else if (!strcmp(argv[2], "--prototypes")) result = inspect_prototypes();
    else if (!strcmp(argv[2], "--create")) result = create_fixture();
    else if (argc == 4 && !strcmp(argv[3], "--ce-shield")) result = check_ce_shield(argv[2]);
    else if (argc == 4 && !strcmp(argv[3], "--ce-portrait")) result = check_ce_portrait_animation(argv[2]);
    else if (argc == 4 && !strcmp(argv[3], "--ce-inspect")) result = inspect_ce_fixture(argv[2]);
    else if (argc == 4 && !strcmp(argv[3], "--ce-isle-area")) result = check_ce_isle_area(argv[2]);
    else if (argc == 4 && !strcmp(argv[3], "--arachnid")) result = arachnid_roundtrip(argv[2], false);
    else if (argc == 4 && !strcmp(argv[3], "--arachnid-original")) result = arachnid_roundtrip(argv[2], true);
    else if (argc == 4 && !strcmp(argv[3], "--save-retry")) result = save_retry_roundtrip(argv[2]);
    else if (argc == 4 && !strcmp(argv[3], "--hardin-route")) result = hardin_route_roundtrip(argv[2]);
    else if (argc == 4) result = roundtrip(argv[2]);
    else {
        result = reward(argv[2], 50, 7000, "SlotC050");
        if (!result) result = reward(argv[2], 73, 11000, "SlotC073");
        if (!result) result = lower_reward(argv[2]);
    }
    gameuilib_exit();
    gamelib_exit();
    tig_exit();
    return result;
}
