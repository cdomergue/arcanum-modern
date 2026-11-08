/* Bounded, transient world-space HP feedback. Never serialized into a save. */
enum {HEALTH_FLOAT_CAPACITY=64,HEALTH_FLOAT_LANES=4,HEALTH_FLOAT_LIFETIME=1200};
typedef struct HealthFloat {
    int64_t actor,delta;
    ObjectID identity;
    Uint64 born;
    int lane;
} HealthFloat;
static HealthFloat health_floats[HEALTH_FLOAT_CAPACITY];

void modern_health_reset(void)
{
    memset(health_floats,0,sizeof(health_floats));
}

static bool health_actor_visible(int64_t actor,TigRect* rect)
{
    if(!actor || !obj_handle_is_valid(actor)
        || !obj_type_is_critter(obj_field_int32_get(actor,OBJ_F_TYPE)))return false;
    if(obj_field_int32_get(actor,OBJ_F_FLAGS)&(OF_OFF|OF_DESTROYED|OF_EXTINCT|OF_DONTDRAW|OF_INVISIBLE))return false;
    if(obj_field_int32_get(actor,OBJ_F_SPELL_FLAGS)&OSF_INVISIBLE)return false;
    int64_t pc=player_get_local_pc_obj();
    if(critter_is_concealed(actor) && actor!=pc && critter_pc_leader_get(actor)!=pc)return false;
    if(roof_is_covered_loc(obj_field_int64_get(actor,OBJ_F_LOCATION),true))return false;
    object_get_rect(actor,0,rect);
    return rect->width>0 && rect->height>0 && rect->x<hrp_iso_window_width_get()
        && rect->y<hrp_iso_window_height_get() && (int64_t)rect->x+rect->width>0
        && (int64_t)rect->y+rect->height>0;
}

void modern_health_change(int64_t actor,int old_damage,int new_damage)
{
    if(old_damage==new_damage || !enabled || !in_game || gamelib_in_load()
        || gamelib_in_reset() || mainmenu_ui_is_active() || tig_movie_is_playing())return;
    TigRect rect;if(!health_actor_visible(actor,&rect))return;
    /* HP_DAMAGE also stores a 32000 death sentinel. Show actual live HP changes,
     * including only the points restored by resurrection, never sentinel writes. */
    int maximum=object_hp_max(actor);if(maximum<=0)return;
    int64_t before=(int64_t)maximum-old_damage,after=(int64_t)maximum-new_damage;
    if(before<0)before=0;
    if(before>maximum)before=maximum;
    if(after<0)after=0;
    if(after>maximum)after=maximum;
    int64_t delta=after-before;if(!delta)return;
    Uint64 now=SDL_GetTicks();
    int slot=-1,oldest=0,actor_oldest=-1;
    unsigned lanes=0;
    for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++){
        HealthFloat* f=&health_floats[i];
        if(f->delta && now-f->born>=HEALTH_FLOAT_LIFETIME)f->delta=0;
        if(!f->delta){if(slot<0)slot=i;continue;}
        if(f->born<health_floats[oldest].born)oldest=i;
        if(f->actor==actor){
            lanes|=1U<<f->lane;
            if(actor_oldest<0 || f->born<health_floats[actor_oldest].born)actor_oldest=i;
        }
    }
    int lane=0;while(lane<HEALTH_FLOAT_LANES && (lanes&(1U<<lane)))lane++;
    if(lane==HEALTH_FLOAT_LANES){slot=actor_oldest;lane=health_floats[slot].lane;}
    if(slot<0)slot=oldest;
    health_floats[slot]=(HealthFloat){actor,delta,obj_get_id(actor),now,lane};
}

static void draw_floating_health(void)
{
    if(!visible() || journal || native_panel() || gamelib_in_load() || gamelib_in_reset())return;
    int width=hrp_iso_window_width_get(),height=hrp_iso_window_height_get();
    if(width<=0 || height<=0)return;
    float sx=canvas_w/width,sy=canvas_h/height;
    Uint64 now=SDL_GetTicks();
    for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++){
        HealthFloat* f=&health_floats[i];if(!f->delta)continue;
        Uint64 age=now-f->born;
        if(age>=HEALTH_FLOAT_LIFETIME){f->delta=0;continue;}
        TigRect rect;
        if(!health_actor_visible(f->actor,&rect)
            || !objid_is_equal(f->identity,obj_get_id(f->actor))){f->delta=0;continue;}
        float phase=(float)age/HEALTH_FLOAT_LIFETIME;
        unsigned char alpha=age<400?255:(unsigned char)(255*(HEALTH_FLOAT_LIFETIME-age)/800);
        char label[32];snprintf(label,sizeof(label),"%+lld",(long long)f->delta);
        float x=(rect.x+rect.width*.5f)*sx-text_measure(2,label)*.5f;
        float y=rect.y*sy-fonts[2].size-8-f->lane*(fonts[2].size+3)-36*phase;
        SDL_Color color=f->delta<0?(SDL_Color){255,83,73,alpha}:(SDL_Color){104,240,128,alpha};
        SDL_Color shadow={12,8,6,alpha};
        text_draw(2,label,x-1,y,shadow);text_draw(2,label,x+1,y,shadow);
        text_draw(2,label,x,y-1,shadow);text_draw(2,label,x,y+1,shadow);
        text_draw(2,label,x,y,color);
    }
}
