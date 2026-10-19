enum {TOWN_ROUTE_LIMIT=6, TOWN_ROUTE_SPAN=32, TOWN_ROUTE_PROBES=128};
typedef struct TownRouteSearch {
    int64_t actor;
    int probes;
    Uint64 deadline;
} TownRouteSearch;

static int64_t town_route_distance(int64_t from,int64_t to)
{
    int64_t dx=(int64_t)LOCATION_GET_X(to)-LOCATION_GET_X(from);
    int64_t dy=(int64_t)LOCATION_GET_Y(to)-LOCATION_GET_Y(from);
    if(dx<0)dx=-dx;
    if(dy<0)dy=-dy;
    return dx>dy?dx:dy;
}

static int town_route_plan(TownRouteSearch* search,int64_t from,int64_t to,int slots,int64_t* points)
{
    int64_t distance=town_route_distance(from,to);
    if(slots<=0 || !distance || distance>TOWN_ROUTE_SPAN*slots
        || search->probes>=TOWN_ROUTE_PROBES || SDL_GetTicks()>=search->deadline)return 0;
    if(distance<=TOWN_ROUTE_SPAN) {
        search->probes++;
        if(anim_preview_route_segment(search->actor,from,to)){points[0]=to;return 1;}
    }
    if(slots==1)return 0;
    int64_t dx=(int64_t)LOCATION_GET_X(to)-LOCATION_GET_X(from);
    int64_t dy=(int64_t)LOCATION_GET_Y(to)-LOCATION_GET_Y(from);
    int64_t stride=distance>TOWN_ROUTE_SPAN?TOWN_ROUTE_SPAN:distance/2;
    if(!stride)return 0;
    int64_t x=LOCATION_GET_X(from)+dx*stride/distance;
    int64_t y=LOCATION_GET_Y(from)+dy*stride/distance;
    const int offsets[][2]={{0,0},{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};
    for(int candidate=0;candidate<25;candidate++) {
        if(search->probes>=TOWN_ROUTE_PROBES || SDL_GetTicks()>=search->deadline)return 0;
        int index=candidate?(candidate-1)%8+1:0;
        int radius=candidate?((candidate-1)/8+1)*4:0;
        int64_t nx=x+offsets[index][0]*radius,ny=y+offsets[index][1]*radius;
        if(nx<0 || ny<0 || nx>UINT32_MAX || ny>UINT32_MAX)continue;
        int64_t next=location_make(nx,ny);
        int64_t remaining=town_route_distance(next,to);
        if(next==from || !remaining || remaining>=distance || remaining>TOWN_ROUTE_SPAN*(slots-1)
            || town_route_distance(from,next)>TOWN_ROUTE_SPAN)continue;
        search->probes++;
        if(!anim_preview_route_segment(search->actor,from,next))continue;
        int count=town_route_plan(search,next,to,slots-1,points+1);
        if(count){points[0]=next;return count+1;}
    }
    return 0;
}

static bool modern_town_route_add(WmapCoords* coords)
{
    WmapRoute* route=&wmap_ui_routes[WMAP_ROUTE_TYPE_TOWN];
    int64_t actor=player_get_local_pc_obj();
    int64_t from=route->length?route->waypoints[route->length-1].loc:obj_field_int64_get(actor,OBJ_F_LOCATION);
    int64_t to,points[TOWN_ROUTE_LIMIT];
    townmap_coords_to_loc(&wmap_ui_tmi,coords->x,coords->y,&to);
    if(from==to)return false;
    TownRouteSearch search={.actor=actor,.deadline=SDL_GetTicks()+250};
    int slots=TOWN_ROUTE_LIMIT-route->length;
    int count=town_route_plan(&search,from,to,slots,points);
    if(!count) {
        MesFileEntry entry={.num=slots<=0 || town_route_distance(from,to)>TOWN_ROUTE_SPAN*slots?611:610};
        mes_get_msg(wmap_ui_worldmap_mes_file,&entry);
        UiMessage message={.type=UI_MSG_TYPE_FEEDBACK,.str=entry.str};
        intgame_message_window_display_msg(&message);
        return false;
    }
    for(int i=0;i<count;i++) {
        int index=route->length;
        WmapRouteWaypoint* waypoint=&route->waypoints[index];
        *waypoint=(WmapRouteWaypoint){.loc=points[i],.steps=1,.field_4=-1};
        townmap_loc_to_coords(&wmap_ui_tmi,points[i],&waypoint->coords.x,&waypoint->coords.y);
        if(i==count-1)waypoint->coords=*coords;
        dword_5C9AD8=index;
        sub_5648E0(index,route->field_3C4,false);
        route->length++;
    }
    sub_5649C0();
    wmap_ui_mode_info[wmap_ui_mode].refresh();
    return true;
}
