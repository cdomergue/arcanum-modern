static int64_t town_travel_actor,town_travel_destination;
static int town_travel_map;
static Uint64 town_travel_idle_since;

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
    if(!anim_goal_move_to_tile(actor,route->waypoints[0].loc)) {
        town_route_feedback();return true;
    }
    bool queued=true;
    for(int i=1;i<route->length;i++) {
        if(!sub_433A00(actor,route->waypoints[i].loc,tig_net_is_active() && !tig_net_is_host())) {
            queued=false;break;
        }
    }
    town_travel_actor=queued && !tig_net_is_active()?actor:OBJ_HANDLE_NULL;
    town_travel_destination=route->waypoints[route->length-1].loc;
    town_travel_map=map_current_map();town_travel_idle_since=0;
    sub_436D20(0x80000,0);wmap_ui_close();
    if(!queued)town_route_feedback();
    return true;
}

void modern_map_route_tick(void)
{
    if(!town_travel_actor)return;
    int64_t actor=town_travel_actor;
    if(!modern_ui_enabled() || !obj_handle_is_valid(actor) || player_get_local_pc_obj()!=actor
        || map_current_map()!=town_travel_map || combat_critter_is_combat_mode_active(actor)) {
        town_travel_actor=OBJ_HANDLE_NULL;return;
    }
    if(obj_field_int64_get(actor,OBJ_F_LOCATION)==town_travel_destination) {
        town_travel_actor=OBJ_HANDLE_NULL;return;
    }
    if(sub_423300(actor,NULL) && !anim_is_idle(actor)){town_travel_idle_since=0;return;}
    if(!town_travel_idle_since){town_travel_idle_since=SDL_GetTicks();return;}
    if(SDL_GetTicks()-town_travel_idle_since>=750) {
        town_travel_actor=OBJ_HANDLE_NULL;town_route_feedback();
    }
}

void modern_map_route_cancel_tracking(void)
{
    town_travel_actor=OBJ_HANDLE_NULL;town_travel_idle_since=0;
}
