/* Inspection is independent of attack eligibility and whose turn it is. Never
   execute examine scripts from this polling path or change the attack target. */
static const ModernDetails* modern_combat_hover_details(int64_t viewer)
{
    static ModernDetails details;
    static int64_t previous_object,previous_viewer;
    static Uint64 refreshed;
    TigMouseState mouse;
    TargetDescriptor target;
    if(modern_details_capture || map_is_clearing_objects()
        || intgame_mode_get()==INTGAME_MODE_DIALOG
        || tig_mouse_get_state(&mouse)!=TIG_OK || !sub_5518C0(mouse.x,mouse.y)
        || !target_pick_at_screen_xy_ex(mouse.x,mouse.y,&target,
            TGT_OBJECT | TGT_OBJ_NO_T_WALL,intgame_fullscreen)
        || target.is_loc || !target.obj || !obj_handle_is_valid(target.obj)) {
        previous_object=OBJ_HANDLE_NULL;
        return NULL;
    }
    int64_t object=target.obj;
    if(!obj_type_is_critter(obj_field_int32_get(object,OBJ_F_TYPE))) {
        previous_object=OBJ_HANDLE_NULL;
        /* Keep script-provided descriptions of items and scenery, but never
           show a cached description belonging to a different hovered object. */
        return object==modern_hover_object && modern_hover_snapshot.count?&modern_hover_snapshot:NULL;
    }
    Uint64 now=SDL_GetTicks();
    if(object!=previous_object || viewer!=previous_viewer || now-refreshed>=100) {
        details=(ModernDetails){.icon=-1,.portrait=-1};
        char name[MAX_STRING];object_examine(object,viewer,name);
        snprintf(details.title,sizeof(details.title),"%s",name);
        bool passthrough=modern_details_passthrough;
        modern_details_capture=&details;modern_details_passthrough=false;
        intgame_examine_critter(viewer,object,name);
        modern_details_capture=NULL;modern_details_passthrough=passthrough;
        previous_object=object;previous_viewer=viewer;refreshed=now;
    }
    return details.count?&details:NULL;
}
