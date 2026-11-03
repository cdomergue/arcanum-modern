/* Handoffs keep the current actor, initiative and remaining action points.
 * Never interrupt a running goal: apply at its completion before AI reschedules. */
bool combat_companion_ai_get(void) { return companion_ai_requested; }
bool combat_companion_ai_pending(void) { return companion_ai_enabled!=companion_ai_requested; }

void combat_companion_ai_set(bool enabled)
{
    if(tactical_enabled && combat_turn_based_active && !tig_net_is_active())
        companion_ai_requested=enabled;
}

static bool combat_companion_ai_apply(void)
{
    if(!combat_companion_ai_pending() || !tactical_enabled || !combat_turn_based_active
        || gamelib_in_load() || !dword_5FC240)return false;
    int64_t actor=dword_5FC240->obj;
    bool companion=combat_party_control_eligible(actor) && !player_is_local_pc_obj(actor);
    if(companion) {
        AnimID current,fidget;
        if(sub_423300(actor,&current) && (!anim_is_current_goal_type(actor,AG_ANIM_FIDGET,&fidget)
            || !anim_id_is_equal(&current,&fidget)))return false;
    }
    shared_turn_store();
    companion_ai_enabled=companion_ai_requested;
    if(companion) {
        /* Clear the AI's no-progress detector without granting another turn. */
        qword_5FC258=OBJ_HANDLE_NULL;dword_5FC260=0;dword_5FC264=false;
        combat_callbacks.field_C(companion_ai_enabled?0:combat_action_points);
        if(companion_ai_enabled) {
            sub_4BB910();
            if(sub_4BB900())sub_4BB8F0();
        } else if(!sub_4BB900())sub_4BB8E0();
    }
    return true;
}

void combat_companion_ai_sync(void)
{
    if(combat_companion_ai_apply() && companion_ai_enabled && dword_5FC240
        && combat_party_control_eligible(dword_5FC240->obj)
        && !player_is_local_pc_obj(dword_5FC240->obj))sub_4B7010(dword_5FC240->obj);
}
