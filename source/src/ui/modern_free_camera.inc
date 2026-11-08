/* One immediate, broad camera advance when the moving actor reaches an edge. */
static void free_camera_log(const char* event,int64_t actor, int64_t dx, int64_t dy, int width, int height,
    int64_t step_x, int64_t step_y,int frames,int duration)
{
    static Uint64 window;
    static int count;
    Uint64 now=SDL_GetTicks();
    if(now-window>=1000){window=now;count=0;}
    if(count++>=20)return;
    int saved_errno=errno;
    FILE* log=fopen("ArcanumModern/free-camera.log","ab+");
    if(log){
        fseek(log,0,SEEK_END);
        if(ftell(log)>=1024*1024){
            fclose(log);log=NULL;
            if(remove("ArcanumModern/free-camera.previous.log")==0 || errno==ENOENT)
                if(rename("ArcanumModern/free-camera.log","ArcanumModern/free-camera.previous.log")==0)
                    log=fopen("ArcanumModern/free-camera.log","ab");
        }
        if(log){
            int64_t after_x,after_y;
            after_x=after_y=0;
            if(obj_handle_is_valid(actor)){
                location_calc_dist_from_screen_center(obj_field_int64_get(actor,OBJ_F_LOCATION),&after_x,&after_y);
                after_x-=obj_field_int32_get(actor,OBJ_F_OFFSET_X);
                after_y-=obj_field_int32_get(actor,OBJ_F_OFFSET_Y);
            }
            fprintf(log,"ticks_ms=%llu event=%s actor=%llu viewport=%dx%d before=%lld,%lld shift=%lld,%lld after=%lld,%lld frames=%d duration_ms=%d\n",
                (unsigned long long)now,event,(unsigned long long)actor,width,height,
                (long long)dx,(long long)dy,(long long)step_x,(long long)step_y,(long long)after_x,(long long)after_y,frames,duration);
            fclose(log);
        }
    }
    errno=saved_errno;
}


static bool free_camera_eligible(int64_t actor)
{
    return !follow_camera && actor && obj_handle_is_valid(actor) && actor==follow_control_actor()
        && world_zoom_allowed() && !menu_suspended && !slide_ui_is_active() && !tactical_active();
}

void modern_free_camera_keep_visible(int64_t actor)
{
    if(!free_camera_eligible(actor))return;
    int width=hrp_iso_window_width_get(),height=hrp_iso_window_height_get();
    if(width<=0 || height<=0)return;
    int64_t dx,dy;
    location_calc_dist_from_screen_center(obj_field_int64_get(actor,OBJ_F_LOCATION),&dx,&dy);
    dx-=obj_field_int32_get(actor,OBJ_F_OFFSET_X);
    dy-=obj_field_int32_get(actor,OBJ_F_OFFSET_Y);
    /* Crossing the outer margin advances the actor to the opposite side of
     * the center, leaving a useful stretch of scenery in front of them. */
    int64_t limit_x=(int64_t)width*35/100,limit_y=(int64_t)height*30/100;
    if(llabs(dx)<=limit_x && llabs(dy)<=limit_y)return;
    /* Advance both useful axes together. A diagonal walk must not provoke
     * separate horizontal and vertical pans a moment apart. */
    int64_t shift_x=dx>width/10?dx+width/5:dx < -width/10?dx-width/5:0;
    int64_t shift_y=dy>height/10?dy+height*15/100:dy < -height/10?dy-height*15/100:0;
    if(llabs(shift_x)<width && llabs(shift_y)<height)scroll_by(shift_x,shift_y);
    else {location_origin_scroll(shift_x,shift_y);iso_invalidate_rect(NULL);}
    free_camera_log("jump",actor,dx,dy,width,height,shift_x,shift_y,1,0);
}
