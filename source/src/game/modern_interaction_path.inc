/* Approach fixed interactives using reachable standing tiles. Never use the
 * adjacency shortcut, which omits the target edge (and can omit its wall). */
bool anim_has_interaction_path(int64_t target)
{
    if (!target || !obj_handle_is_valid(target)) return false;
    int type = obj_field_int32_get(target, OBJ_F_TYPE);
    return type == OBJ_TYPE_PORTAL || type == OBJ_TYPE_CONTAINER
        || type == OBJ_TYPE_SCENERY || (obj_type_is_critter(type) && critter_is_dead(target));
}

static int anim_interaction_range(int64_t target)
{
    int type = obj_field_int32_get(target, OBJ_F_TYPE);
    return type == OBJ_TYPE_CONTAINER || obj_type_is_critter(type) ? 1 : 2;
}

/* Reaching to an object is not movement: an intermediate door must already
 * be open and an intact window must actually be crossed by the movement goal. */
static bool anim_interaction_edge(int64_t actor, int64_t from, int64_t next, int rot, int64_t target)
{
    if (tile_is_blocking(next, false)) return false;
    int saved_flags = obj_field_int32_get(target, OBJ_F_FLAGS);
    bool hide_portal = obj_field_int32_get(target, OBJ_F_TYPE) == OBJ_TYPE_PORTAL
        && !(saved_flags & OF_OFF);
    /* A closed/locked portal may itself be used. Hide only that target while
     * checking all four legs of a diagonal, so other barriers still count. */
    /* Do not call object_flags_set(OF_OFF): it removes text/lights. This
     * synchronous traversal query only needs the raw visibility filter. */
    if (hide_portal) obj_field_int32_set(target, OBJ_F_FLAGS, saved_flags | OF_OFF);
    int64_t block = OBJ_HANDLE_NULL;
    int type = OBJ_TYPE_WALL;
    bool window = false;
    int cost = object_calc_traversal_cost(actor, from, rot,
        OBJ_TRAVERSAL_SKIP_OBJECTS | OBJ_TRAVERSAL_PORTAL_BLOCKS | OBJ_TRAVERSAL_WINDOW_BLOCKS,
        &block, &type, &window);
    if (hide_portal) obj_field_int32_set(target, OBJ_F_FLAGS, saved_flags);
    return !cost && !block && !window;
}

static bool anim_interaction_reachable(int64_t actor, int64_t from, int64_t target, int range)
{
    int64_t to = obj_field_int64_get(target, OBJ_F_LOCATION);
    if ((obj_field_int32_get(target, OBJ_F_FLAGS) & (OF_OFF | OF_DESTROYED))
        || location_dist(from, to) > range) return false;
    for (int step = 0; from != to && step < range; step++) {
        int rot = location_rot(from, to);
        int64_t next;
        if (!location_in_dir(from, rot, &next)
            || !anim_interaction_edge(actor, from, next, rot, target)) return false;
        from = next;
    }
    return from == to;
}

static void anim_interaction_log(int64_t actor, int64_t target, int range, int candidates,
    int steps, const char* outcome, tig_duration_t elapsed)
{
    if (!player_is_local_pc_obj(actor) && !combat_player_controls(actor)) return;
    static Uint64 window;
    static int count;
    Uint64 now = SDL_GetTicks();
    if (now - window >= 1000) { window = now; count = 0; }
    if (count++ >= 20) return;
    int saved_errno = errno;
    FILE* log = fopen("ArcanumModern/interaction-path.log", "ab+");
    if (log) {
        fseek(log, 0, SEEK_END);
        if (ftell(log) >= 1024 * 1024) {
            fclose(log);
            log = NULL;
            if (remove("ArcanumModern/interaction-path.previous.log") == 0 || errno == ENOENT) {
                if (rename("ArcanumModern/interaction-path.log", "ArcanumModern/interaction-path.previous.log") == 0)
                    log = fopen("ArcanumModern/interaction-path.log", "ab");
            }
        }
        if (log) {
            fprintf(log, "ticks_ms=%llu actor=%llu target=%llu combat=%d range=%d candidates=%d steps=%d outcome=%s elapsed_ms=%d\n",
                (unsigned long long)now, (unsigned long long)actor, (unsigned long long)target,
                combat_turn_based_is_active(), range, candidates, steps, outcome, (int)elapsed);
            fclose(log);
        }
    }
    errno = saved_errno;
}

static bool anim_plan_interaction(int64_t actor, int64_t target, int range, AnimPath* path, bool diagnostic)
{
    tig_timestamp_t started;
    tig_timer_now(&started);
    path->max = 0;
    path->curr = 0;
    if (!actor || !obj_handle_is_valid(actor) || !anim_has_interaction_path(target)
        || range < 1 || range > 2
        || (obj_field_int32_get(target, OBJ_F_FLAGS) & (OF_DESTROYED | OF_OFF))) return false;
    int64_t from = obj_field_int64_get(actor, OBJ_F_LOCATION);
    int64_t to = obj_field_int64_get(target, OBJ_F_LOCATION);
    path->field_CC = sizeof(path->rotations);
    path->maxPathLength = path->absMaxPathLength = path->field_CC;
    path->field_E8 = from;
    path->field_F0 = from;
    path->flags &= ~0x03;
    if (anim_interaction_reachable(actor, from, target, range)) return true;

    PathCreateInfo info = {0};
    info.obj = actor;
    info.from = from;
    info.to = to;
    info.max_rotations = sizeof(path->rotations);
    info.rotations = path->rotations;
    if (!sub_425BF0(&info, false)) return false;
    /* Preserve completion of a step already in progress, as anim_find_path does. */
    int prefix = 0;
    if (obj_field_int32_get(actor, OBJ_F_OFFSET_X) || obj_field_int32_get(actor, OBJ_F_OFFSET_Y)) {
        int rot = tig_art_id_rotation_get(obj_field_int32_get(actor, OBJ_F_CURRENT_AID));
        int64_t next;
        if (!location_in_dir(from, rot, &next) || tile_is_blocking(next, false)
            || trap_party_avoids_location(actor, next)
            || object_traversal_is_blocked(actor, from, rot, path_translate_flags(info.flags), NULL)) return false;
        path->rotations[0] = rot;
        info.from = next;
        info.rotations++;
        info.max_rotations--;
        prefix = 1;
    }
    int64_t goals[25];
    int count = 0;
    for (int y = -range; y <= range; y++) {
        for (int x = -range; x <= range; x++) {
            int64_t gx = LOCATION_GET_X(to) + x, gy = LOCATION_GET_Y(to) + y;
            if ((!x && !y) || gx < 0 || gy < 0) continue;
            int64_t goal = location_make(gx, gy);
            if (!tile_is_blocking(goal, false) && anim_interaction_reachable(actor, goal, target, range))
                goals[count++] = goal;
        }
    }
    int steps = path_make_to_goals(&info, goals, count);
    bool found = steps > 0 || (prefix && anim_interaction_reachable(actor, info.from, target, range));
    if (found) {
        path->max = steps + prefix;
        path->field_F0 = steps ? info.to : info.from;
    }
    if (diagnostic) anim_interaction_log(actor, target, range, count, path->max,
        found ? "found" : count ? "unreachable_or_budget" : "no_interaction_tile", tig_timer_elapsed(started));
    return found;
}

bool anim_preview_interaction_path(int64_t actor, int64_t target, AnimPath* path)
{
    memset(path, 0, sizeof(*path));
    return anim_has_interaction_path(target)
        && anim_plan_interaction(actor, target, anim_interaction_range(target), path, true);
}

static bool anim_move_to_interaction(AnimRunInfo* run, int range)
{
    bool found = anim_plan_interaction(run->params[0].obj, run->params[1].obj, range, &run->path, true);
    if (found) {
        if (run->path.max > 8) run->path.max = 8;
        /* Native movement compares this field with the object's location to
         * invalidate paths when the object moves; field_F0 stores our endpoint. */
        run->cur_stack_data->params[AGDATA_TARGET_TILE].loc = obj_field_int64_get(run->params[1].obj, OBJ_F_LOCATION);
    } else {
        modern_combat_rejected(run->params[0].obj, run->params[1].obj, 2, 0);
    }
    return found;
}
