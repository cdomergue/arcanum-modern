/* Persistent event diagnostics, never per-frame. No names or save contents. */
typedef struct CombatDiagnosticActor {
    int valid, type, hp, fatigue, mode, slot, goal, shield;
    unsigned int spells, flags, flags2;
    int64_t obj, focus;
} CombatDiagnosticActor;

static CombatDiagnosticActor combat_diagnostic_snapshot(int64_t obj)
{
    CombatDiagnosticActor state = {0};
    state.obj = obj;
    state.type = state.slot = state.goal = state.shield = -1;
    if (!obj || !obj_handle_is_valid(obj)) return state;
    state.valid = 1;
    state.type = obj_field_int32_get(obj, OBJ_F_TYPE);
    state.spells = obj_field_int32_get(obj, OBJ_F_SPELL_FLAGS);
    if (obj_type_is_critter(state.type)) {
        state.hp = object_hp_current(obj);
        state.fatigue = critter_fatigue_current(obj);
        state.flags = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS);
        state.flags2 = obj_field_int32_get(obj, OBJ_F_CRITTER_FLAGS2);
        state.mode = combat_critter_is_combat_mode_active(obj);
        if (state.type == OBJ_TYPE_NPC)
            state.focus = obj_field_handle_get(obj, OBJ_F_NPC_COMBAT_FOCUS);
        AnimID id;
        AnimRunInfo* run;
        if (sub_423300(obj, &id) && anim_id_to_run_info(&id, &run)) {
            state.slot = id.slot_num;
            if (run->current_goal >= 0) state.goal = run->goals[run->current_goal].type;
        }
    }
    int index;
    if ((state.spells & OSF_SHIELDED) && sub_459170(obj, OSF_SHIELDED, &index))
        state.shield = magictech_run_info[index].spell;
    return state;
}

static void combat_diagnostic_actor_fields(FILE* log, const char* role,
    const CombatDiagnosticActor* state)
{
    fprintf(log, " %s={obj=%llu valid=%d type=%d hp=%d fatigue=%d mode=%d focus=%llu slot=%d goal=%d spells=%x shield=%d flags=%x flags2=%x}",
        role, (unsigned long long)state->obj, state->valid, state->type,
        state->hp, state->fatigue, state->mode, (unsigned long long)state->focus,
        state->slot, state->goal, state->spells, state->shield, state->flags, state->flags2);
}

static unsigned long long combat_diagnostic_sequence;

static void combat_diagnostic_event(const char* event, const CombatContext* combat,
    const char* detail, const CombatDiagnosticActor* before_a,
    const CombatDiagnosticActor* before_t)
{
    int saved_errno = errno;
    CombatDiagnosticActor attacker = combat_diagnostic_snapshot(combat->attacker_obj);
    CombatDiagnosticActor target = combat_diagnostic_snapshot(combat->target_obj);
    time_t now = time(NULL);
    struct tm* local = localtime(&now);
    char stamp[32] = "time-unavailable";
    if (local) strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", local);
    unsigned long long sequence = ++combat_diagnostic_sequence;
    FILE* log = fopen("ArcanumModern/combat-events.log", "ab+");
    if (log) {
        fseek(log, 0, SEEK_END);
        if (ftell(log) >= 1024 * 1024) {
            fclose(log);
            remove("ArcanumModern/combat-events.previous.log");
            if (rename("ArcanumModern/combat-events.log", "ArcanumModern/combat-events.previous.log") != 0) {
                errno = saved_errno;
                return;
            }
            log = fopen("ArcanumModern/combat-events.log", "ab");
        }
        if (log) {
            fprintf(log, "%s seq=%llu event=%s tb=%d flags=%x damage_flags=%x weapon=%llu original_target=%llu damage=%d,%d,%d,%d,%d detail=%.256s",
                stamp, sequence, event, combat_turn_based_is_active(), combat->flags,
                combat->dam_flags, (unsigned long long)combat->weapon_obj,
                (unsigned long long)combat->field_28,
                combat->dam[0], combat->dam[1], combat->dam[2], combat->dam[3], combat->dam[4],
                detail ? detail : "");
            if (before_a) combat_diagnostic_actor_fields(log, "before_a", before_a);
            if (before_t) combat_diagnostic_actor_fields(log, "before_t", before_t);
            combat_diagnostic_actor_fields(log, "a", &attacker);
            combat_diagnostic_actor_fields(log, "t", &target);
            fputc('\n', log);
            fclose(log);
        }
    }
    errno = saved_errno;
}

static void combat_dmg_impl(CombatContext* combat);

void combat_dmg(CombatContext* combat)
{
    int saved_errno = errno;
    CombatDiagnosticActor attacker = combat_diagnostic_snapshot(combat->attacker_obj);
    CombatDiagnosticActor target = combat_diagnostic_snapshot(combat->target_obj);
    char detail[160];
    snprintf(detail, sizeof(detail), "input_flags=%x input_damage_flags=%x input_damage=%d,%d,%d,%d,%d",
        combat->flags, combat->dam_flags, combat->dam[0], combat->dam[1],
        combat->dam[2], combat->dam[3], combat->dam[4]);
    errno = saved_errno;
    combat_dmg_impl(combat);
    combat_diagnostic_event("damage", combat, detail, &attacker, &target);
}
