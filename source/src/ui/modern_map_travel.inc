static int64_t town_travel_actor,town_travel_destination;
static int64_t* town_travel_points;
static int town_travel_count,town_travel_next,town_travel_map;
static Uint64 town_travel_idle_since;

void modern_map_route_cancel_tracking(void)
{
    free(town_travel_points);town_travel_points=NULL;
    town_travel_count=town_travel_next=0;
    town_travel_actor=OBJ_HANDLE_NULL;town_travel_idle_since=0;
}

static void town_route_feedback(void)
{
    MesFileEntry entry={.num=610};mes_get_msg(wmap_ui_worldmap_mes_file,&entry);
    UiMessage message={.type=UI_MSG_TYPE_FEEDBACK,.str=entry.str};
    intgame_message_window_display_msg(&message);
}

static bool modern_town_route_start(void)
{
    WmapRoute* route=&wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN];
    int64_t actor=player_get_local_pc_obj();
    int64_t from=obj_field_int64_get(actor,OBJ_F_LOCATION);
    if(!route->length)return true;
    for(int i=0;i<route->length;i++) {
        if(!anim_preview_route_segment(actor,from,route->waypoints[i].loc)) {
            town_route_feedback();return true;
        }
        from=route->waypoints[i].loc;
    }
    /* Own a snapshot: reopening or editing the map must not mutate a journey. */
    int64_t* points=malloc((size_t)route->length*sizeof(*points));
    if(!points){town_route_feedback();return true;}
    for(int i=0;i<route->length;i++)points[i]=route->waypoints[i].loc;
    if(!anim_goal_move_to_tile(actor,points[0])) {
        free(points);town_route_feedback();return true;
    }
    modern_map_route_cancel_tracking();
    town_travel_points=points;town_travel_count=route->length;town_travel_next=1;
    town_travel_actor=actor;town_travel_destination=points[0];
    town_travel_map=map_current_map();town_travel_idle_since=0;
    sub_436D20(0x80000,0);wmap_ui_close();
    return true;
}

void modern_map_route_tick(void)
{
    if(!town_travel_actor)return;
    int64_t actor=town_travel_actor;
    if(!obj_handle_is_valid(actor) || player_get_local_pc_obj()!=actor
        || map_current_map()!=town_travel_map || combat_critter_is_combat_mode_active(actor)) {
        modern_map_route_cancel_tracking();return;
    }
    /* Let the current movement finish before feeding the next segment. This
       avoids the native animation stack's six-waypoint limit. */
    if(sub_423300(actor,NULL) && !anim_is_idle(actor)){town_travel_idle_since=0;return;}
    if(obj_field_int64_get(actor,OBJ_F_LOCATION)==town_travel_destination) {
        if(town_travel_next==town_travel_count){modern_map_route_cancel_tracking();return;}
        int64_t next=town_travel_points[town_travel_next];
        if(!anim_preview_route_segment(actor,town_travel_destination,next)
            || !anim_goal_move_to_tile(actor,next)) {
            modern_map_route_cancel_tracking();town_route_feedback();return;
        }
        /* A new movement animation does not inherit the previous camera flag. */
        sub_436D20(0x80000,0);
        town_travel_destination=next;town_travel_next++;town_travel_idle_since=0;
        return;
    }
    if(!town_travel_idle_since){town_travel_idle_since=SDL_GetTicks();return;}
    if(SDL_GetTicks()-town_travel_idle_since>=750) {
        modern_map_route_cancel_tracking();town_route_feedback();
    }
}
