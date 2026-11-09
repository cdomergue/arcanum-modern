/* Coordinate queries must not move the cursor, create routes or load sectors.
 * In town view the old compass width may still be uninitialised: derive it
 * from the current map limits, as the companion journal does from map.prp. */
static bool modern_map_local_limits(int64_t* width,int64_t* height)
{
    location_limits_get(width,height);
    return *width>=64 && *height>=64 && *width<=INT32_MAX && *height<=INT32_MAX;
}
bool modern_map_point_coords(int x,int y,int* west,int* south)
{
    if(!wmap_ui_created)return false;
    WmapInfo* info=&wmap_ui_mode_info[wmap_ui_mode];
    if(wmap_ui_mode==WMAP_UI_MODE_TOWN) {
        int64_t width,height,loc;
        if(!modern_map_local_limits(&width,&height) || wmap_ui_tmi.scale<=0
            || x<0 || y<0 || x>=info->map_width || y>=info->map_height)return false;
        int64_t cx,cy;
        location_xy(wmap_ui_tmi.loc,&cx,&cy);
        if(!location_at(cx+(int64_t)(x/wmap_ui_tmi.scale)-wmap_ui_tmi.width/2,
            cy+(int64_t)(y/wmap_ui_tmi.scale)-wmap_ui_tmi.height/2,&loc))return false;
        int64_t lx=LOCATION_GET_X(loc),ly=LOCATION_GET_Y(loc);
        if(lx>=width || ly>=height)return false;
        *west=(int)(width/64-lx/64);*south=(int)(ly/64);
        return true;
    }
    WmapInfo* world=&wmap_ui_mode_info[WMAP_UI_MODE_WORLD];
    int width=wmap_ui_mode==WMAP_UI_MODE_CONTINENT?info->field_14.width:world->map_width;
    int height=wmap_ui_mode==WMAP_UI_MODE_CONTINENT?info->field_14.height:world->map_height;
    if(width<=0 || height<=0 || world->map_width<=0 || world->map_height<=0
        || x<0 || y<0 || x>=width || y>=height)return false;
    *west=(int)((int64_t)x*world->map_width/width);
    *south=(int)((int64_t)y*world->map_height/height);
    return true;
}
bool modern_map_coords_point(int west,int south,int* x,int* y)
{
    if(!wmap_ui_created || west<0 || south<0)return false;
    WmapInfo* info=&wmap_ui_mode_info[wmap_ui_mode];
    if(wmap_ui_mode==WMAP_UI_MODE_TOWN) {
        int64_t width,height,cx,cy,lx,ly;
        if(!modern_map_local_limits(&width,&height) || wmap_ui_tmi.scale<=0
            || west<1 || west>width/64 || south>=height/64)return false;
        int64_t loc=LOCATION_MAKE((width/64-west)*64+32,((int64_t)south)*64+32);
        location_xy(wmap_ui_tmi.loc,&cx,&cy);location_xy(loc,&lx,&ly);
        lx+=wmap_ui_tmi.width/2-cx;ly+=wmap_ui_tmi.height/2-cy;
        /* townmap_loc_to_coords returns 0,0 on failure; test its bounds first
         * so an out-of-town position cannot become a note in the corner. */
        if(lx<0 || ly<0 || lx>=wmap_ui_tmi.width || ly>=wmap_ui_tmi.height)return false;
        *x=(int)(lx*wmap_ui_tmi.scale);*y=(int)(ly*wmap_ui_tmi.scale);
        return *x>=0 && *y>=0 && *x<info->map_width && *y<info->map_height;
    }
    WmapInfo* world=&wmap_ui_mode_info[WMAP_UI_MODE_WORLD];
    if(world->map_width<=0 || world->map_height<=0 || west>=world->map_width || south>=world->map_height)return false;
    if(wmap_ui_mode==WMAP_UI_MODE_CONTINENT) {
        if(info->field_14.width<=0 || info->field_14.height<=0)return false;
        *x=(int)((int64_t)west*info->field_14.width/world->map_width);
        *y=(int)((int64_t)south*info->field_14.height/world->map_height);
    } else {*x=west;*y=south;}
    return true;
}
bool modern_map_position_coords(int* west,int* south)
{
    if(!wmap_ui_created)return false;
    if(wmap_ui_mode==WMAP_UI_MODE_TOWN) {
        int64_t pc=player_get_local_pc_obj(),width,height;
        if(!pc || !modern_map_local_limits(&width,&height))return false;
        int64_t loc=obj_field_int64_get(pc,OBJ_F_LOCATION);
        if(LOCATION_GET_X(loc)>=width || LOCATION_GET_Y(loc)>=height)return false;
        *west=(int)(width/64-LOCATION_GET_X(loc)/64);*south=(int)(LOCATION_GET_Y(loc)/64);
    } else {
        WmapInfo* info=&wmap_ui_mode_info[WMAP_UI_MODE_WORLD];
        if(info->field_3C.x<0 || info->field_3C.y<0
            || info->field_3C.x>=info->map_width || info->field_3C.y>=info->map_height)return false;
        *west=info->field_3C.x;*south=info->field_3C.y;
    }
    return true;
}
