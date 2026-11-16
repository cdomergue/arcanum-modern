/* Native controller navigation uses the same action IDs as the visible controls.
 * Inventory coordinates address the engine's inventory adapter, never OS input. */
#include <float.h>
enum {PAD_FOCUS_CAPACITY=512};
typedef struct PadFocus { SDL_FRect rect; int action,x,y; } PadFocus;
static PadFocus pad_focus_nodes[PAD_FOCUS_CAPACITY];
static int pad_focus_count,pad_focus_index=-1,pad_context=-1,pad_draw_context=-1;
static int pad_repeat_direction=-1,pad_radial_slot=-1;
static Uint64 pad_repeat_at,pad_tick_time;
static bool pad_radial,pad_cursor_mode,pad_target_stick_blocked;
static bool pad_menu_radial;
static int pad_radial_offset;
static const int pad_menu_actions[]={1,0,3,2,5,6,8,9,4};
static const int pad_menu_labels[]={1005,1004,1007,1006,1001,1000,1003,1009,1008};
static int64_t pad_radial_actor,pad_target;
static ObjectID pad_target_id;
static int pad_options_page;
static QuickBinding pad_binding;
static int pad_binding_slot;
static int pad_target_mode;

static bool pad_aiming(void)
{
    int mode=intgame_mode_get();
    return mode==INTGAME_MODE_SPELL || mode==INTGAME_MODE_SKILL || mode==INTGAME_MODE_ITEM;
}

static bool pad_modal(void)
{
    return native_menu() || journal || native_panel() || album.pages || portrait_editor.active;
}

static int pad_screen(void)
{
    int screen=mainmenu_ui_is_active()?100+mainmenu_ui_window_type_get():native_panel();
    if(journal)screen=20;
    if(confirming_menu)screen=300;
    if(album.pages)screen=301;
    if(portrait_editor.active)screen=302;
    if(intgame_mode_get()==INTGAME_MODE_QUANTITY)screen=303;
    return screen;
}

static void pad_save(void)
{
    FILE* f=fopen("modern-ui/gamepad.ini.tmp","w");if(!f)return;
    fprintf(f,"Enabled=%d\nRumble=%d\nDeadzone=%d\nCursorSpeed=%d\n",
        (int)gamepad.enabled,(int)gamepad.rumble,gamepad.deadzone,gamepad.cursor_speed);
    if(fclose(f)==0)SDL_RenamePath("modern-ui/gamepad.ini.tmp","modern-ui/gamepad.ini");
}

static void pad_load(void)
{
    FILE* f=fopen("modern-ui/gamepad.ini","r");if(!f)return;
    char line[128];int value;
    while(fgets(line,sizeof(line),f)) {
        if(sscanf(line,"Enabled=%d",&value)==1 && (value==0 || value==1))gamepad.enabled=value!=0;
        else if(sscanf(line,"Rumble=%d",&value)==1 && (value==0 || value==1))gamepad.rumble=value!=0;
        else if(sscanf(line,"Deadzone=%d",&value)==1)gamepad.deadzone=SDL_clamp(value,5,40);
        else if(sscanf(line,"CursorSpeed=%d",&value)==1)gamepad.cursor_speed=SDL_clamp(value,25,200);
    }
    fclose(f);
}

static void draw_pad_options(float x,float y,float w)
{
    const char* connected=gamepad.device?SDL_GetGamepadName(gamepad.device):tr("Aucune manette connectée","No controller connected");
    button_label(0,connected,(SDL_FRect){x,y+187,w,36},false,MUTED);
    if(!pad_options_page) {
        const char* labels[]={tr("Déplacement / navigation","Movement / navigation"),tr("Curseur / sélection radiale","Pointer / radial selection"),
            tr("Valider / interagir","Confirm / interact"),menu_text(54),key_label(KB_INVENTORY),key_label(KB_COMBAT),
            tr("Raccourcis / barre Alt","Quick slots / Alt bar"),tr("Roue des menus (maintenir)","Menu wheel (hold)"),tr("Menu / journal","Menu / journal"),tr("Dézoomer / zoomer","Zoom out / in")};
        const char* values[]={tr("Stick gauche / croix","Left stick / D-pad"),tr("Stick droit","Right stick"),
            modern_pad_button_name(&gamepad,SDL_GAMEPAD_BUTTON_SOUTH),modern_pad_button_name(&gamepad,SDL_GAMEPAD_BUTTON_EAST),
            modern_pad_button_name(&gamepad,SDL_GAMEPAD_BUTTON_WEST),modern_pad_button_name(&gamepad,SDL_GAMEPAD_BUTTON_NORTH),
            "LB / LB + LT","RB / R1","Start / Back","LT / RT"};
        for(int i=0;i<10;i++) {
            float row=y+230+i*39;option_row(x,row,w,labels[i],true);
            button_label(0,values[i],(SDL_FRect){x+option_label_width(w),row,w-option_label_width(w),36},false,GOLD);
        }
    } else {
        option_row(x,y+239,w,tr("Support manette","Gamepad support"),true);
        option_button((SDL_FRect){x+option_label_width(w),y+242,220,36},463,gamepad.enabled?tr("Activé","Enabled"):tr("Désactivé","Disabled"),false,true);
        option_row(x,y+295,w,tr("Vibrations","Vibration"),true);
        option_button((SDL_FRect){x+option_label_width(w),y+298,220,36},464,gamepad.rumble?tr("Activées","Enabled"):tr("Désactivées","Disabled"),false,true);
        char value[32];snprintf(value,sizeof(value),"%d %%",gamepad.deadzone);
        option_choice(x,y+351,w,tr("Zone morte des sticks","Stick dead zone"),value,465,466,gamepad.deadzone>5,gamepad.deadzone<40,true);
        snprintf(value,sizeof(value),"%d %%",gamepad.cursor_speed);
        option_choice(x,y+407,w,tr("Vitesse du curseur","Pointer speed"),value,467,468,gamepad.cursor_speed>25,gamepad.cursor_speed<200,true);
        menu_paragraph(tr("X (inventaire, sorts, compétences) : affecter un raccourci. Y (inventaire) : utiliser / transférer. Carte : stick gauche. Clic stick droit / gauche : cible suivante / fin du tour. A / B / Start : passer une vidéo.",
            "X (inventory, spells, skills): assign a quick slot. Y (inventory): use / transfer. Map: left stick. Right / left stick click: next target / end turn. A / B / Start: skip a movie."),
            (SDL_FRect){x,y+477,w,162},0,MUTED);
    }
    button((SDL_FRect){x,y+648,36,36},469,"<",-1,false,pad_options_page>0);
    button_label(1,pad_options_page?"2 / 2":"1 / 2",(SDL_FRect){x+40,y+645,66,42},true,GOLD);
    button((SDL_FRect){x+110,y+648,36,36},470,">",-1,false,pad_options_page<1);
    option_button((SDL_FRect){x+w-180,y+648,180,36},471,tr("Par défaut","Defaults"),false,true);
}

static bool pad_options_action(int action)
{
    if(action<463 || action>471)return false;
    if(action==463){gamepad.enabled=!gamepad.enabled;modern_pad_barrier(&gamepad);}
    if(action==464)gamepad.rumble=!gamepad.rumble;
    if(action==465 || action==466)gamepad.deadzone=SDL_clamp(gamepad.deadzone+(action==465?-1:1),5,40);
    if(action==467 || action==468)gamepad.cursor_speed=SDL_clamp(gamepad.cursor_speed+(action==467?-25:25),25,200);
    if(action==469 || action==470)pad_options_page=action==469?0:1;
    if(action==471){gamepad.enabled=gamepad.rumble=true;gamepad.deadzone=18;gamepad.cursor_speed=100;}
    if(!gamepad.rumble || !gamepad.enabled)if(gamepad.device)SDL_RumbleGamepad(gamepad.device,0,0,0);
    pad_save();return true;
}

static void pad_focus_add(SDL_FRect rect,int action,int x,int y)
{
    if(pad_focus_count>=PAD_FOCUS_CAPACITY || rect.w<=0 || rect.h<=0)return;
    pad_focus_nodes[pad_focus_count++]=(PadFocus){rect,action,x,y};
}

static bool pad_graphics(void)
{
    return mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_OPTIONS && options_tab<0;
}

static bool pad_options(void)
{
    return mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_OPTIONS;
}

static bool pad_sheet(void)
{
    return play_sheet() || (mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_CHAREDIT);
}

/* A value has one focus stop even when one of its arrows is disabled. */
static int pad_value_pair(int action)
{
    if(pad_options()) {
        if(pad_graphics()) {
            if(action==410 || action==411)return 410;
            if(action==412 || action==413)return 412;
            if(action==426 || action==427)return 426;
        } else if(options_tab<3 && action>=430 && action<446)return 430+(action-430)/2*2;
        else if(options_tab==3 && key_gamepad) {
            if(action==465 || action==466)return 465;
            if(action==467 || action==468)return 467;
        }
    }
    if(pad_sheet() && (action==640 || action==641))return 640;
    if(intgame_mode_get()==INTGAME_MODE_QUANTITY && (action==760 || action==761))return 760;
    return -1;
}

static int pad_row_action(int action)
{
    if(pad_sheet()) {
        if(action>=620 && action<640)return 690+(action-620)/2;
        if(sheet_category==1 && action>=600 && action<608)return 660+(action-600)/2;
    }
    int pair=pad_value_pair(action);
    return pair>=0?pair+1:action;
}

static bool pad_action_available(int action)
{
    for(int i=0;i<button_count;i++)if(buttons[i].action==action)return buttons[i].enabled;
    return false;
}

static void pad_collect_focus(void)
{
    PadFocus previous={.action=-3};
    if(pad_focus_index>=0 && pad_focus_index<pad_focus_count)previous=pad_focus_nodes[pad_focus_index];
    pad_focus_count=0;pad_focus_index=-1;
    for(int i=0;i<button_count;i++)if(buttons[i].enabled) {
        int action=buttons[i].action;
        /* HUD controls behind the dialogue are not part of its focus scope. */
        if(native_panel()==4 && (action<800 || action>823))continue;
        SDL_FRect rect=buttons[i].rect;
        int row=pad_row_action(action);
        if(row!=action || pad_value_pair(action)>=0
            || (pad_sheet() && ((action>=690 && action<700) || (sheet_category==1 && action>=660 && action<664)))) {
            bool exists=false;
            for(int j=0;j<pad_focus_count;j++)if(pad_focus_nodes[j].action==row)exists=true;
            if(exists)continue;
            for(int j=0;j<button_count;j++)if(pad_row_action(buttons[j].action)==row) {
                SDL_FRect other=buttons[j].rect;
                float right=fmaxf(rect.x+rect.w,other.x+other.w);
                float bottom=fmaxf(rect.y+rect.h,other.y+other.h);
                rect.x=fminf(rect.x,other.x);rect.w=right-rect.x;
                rect.y=fminf(rect.y,other.y);rect.h=bottom-rect.y;
            }
            action=row;
        }
        pad_focus_add(rect,action,0,0);
    }
    if(native_panel()==2 && intgame_mode_get()!=INTGAME_MODE_QUANTITY)for(int i=0;i<panel_region_count;i++) {
        PanelRegion* r=&panel_regions[i];if(r->kind!=2)continue;
        int cols=r->w?10:1, rows=r->h?r->h/32:1;
        for(int row=0;row<rows;row++)for(int col=0;col<cols;col++) {
            SDL_FRect cell={r->rect.x+col*r->rect.w/cols,r->rect.y+row*r->rect.h/rows,r->rect.w/cols,r->rect.h/rows};
            pad_focus_add(cell,r->w?-2:-1,r->x+(r->w?col*32+16:0),r->y+(r->h?row*32+16:0));
        }
    }
    int screen=pad_screen();
    for(int i=0;i<pad_focus_count;i++) {
        PadFocus* n=&pad_focus_nodes[i];
        if(screen==pad_draw_context && n->action==previous.action && n->x==previous.x && n->y==previous.y){pad_focus_index=i;break;}
    }
    pad_draw_context=screen;
    if(pad_focus_index<0 && pad_focus_count)pad_focus_index=0;
}

static void pad_move_focus(int dx,int dy)
{
    SDL_FRect rects[PAD_FOCUS_CAPACITY];
    for(int i=0;i<pad_focus_count;i++)rects[i]=pad_focus_nodes[i].rect;
    int action=pad_focus_index>=0?pad_focus_nodes[pad_focus_index].action:-1,desired=-1;
    int before=pad_focus_index;
    int pair=pad_value_pair(action);
    if(pad_sheet()) {
        if(action>=690 && action<700)pair=620+(action-690)*2;
        if(sheet_category==1 && action>=660 && action<664)pair=600+(action-660)*2;
    }
    bool toggle=pad_options() && ((pad_graphics() && action==429)
        || (options_tab==0 && (action==447 || action==417 || action==419))
        || (options_tab==3 && key_gamepad && (action==463 || action==464)));
    if(dx && (pair>=0 || toggle)) {
        int adjust=pair>=0?pair+(dx>0):action;
        if(pad_action_available(adjust)) {
            pending_action=adjust;modern_pad_log("value-adjust",pad_screen(),adjust);
        }
        return;
    }
    int panel=native_panel();
    bool rows=pad_options() || pad_sheet() || native_menu() || journal
        || (panel>=5 && panel<=9) || intgame_mode_get()==INTGAME_MODE_QUANTITY;
    if(rows && pad_focus_index>=0) {
        if(pad_options() && dx && (action==446 || action==428 || action==471))return;
        if(dy) {
            /* All rows have one vertical stop, independent of column widths.
             * Footer buttons and tabs retain horizontal navigation. */
            float y=rects[pad_focus_index].y+rects[pad_focus_index].h/2,best=FLT_MAX,across=FLT_MAX;
            for(int i=0;i<pad_focus_count;i++) {
                int candidate=pad_focus_nodes[i].action;
                if(pad_sheet() && action>=600 && action<690 && candidate>=690 && candidate<700)continue;
                float step=(rects[i].y+rects[i].h/2-y)*dy;
                float distance=fabsf(rects[i].x+rects[i].w/2-(rects[pad_focus_index].x+rects[pad_focus_index].w/2));
                if(step>8 && (step<best-1 || (fabsf(step-best)<=1 && distance<across))) {
                    best=step;across=distance;desired=pad_focus_nodes[i].action;
                }
            }
        }
    }
    if(pad_sheet() && dy && action>=690 && action<700) {
        /* Traverse both columns before the skill pane; no +/- detour. */
        const int stats[]={692,693,694,695,696,697,698,699,690,691};
        for(int i=0;i<10;i++)if(stats[i]==action) {
            int row=i+dy;
            desired=row<0?3000:row>=10?651:stats[row];break;
        }
    }
    int next=-1;
    if(desired>=0)for(int i=0;i<pad_focus_count;i++)if(pad_focus_nodes[i].action==desired)next=i;
    pad_focus_index=next>=0?next:modern_pad_focus(rects,pad_focus_count,pad_focus_index,dx,dy);
    pad_cursor_mode=false;
    if(pad_focus_index>=0) {
        PadFocus* n=&pad_focus_nodes[pad_focus_index];
        mouse_x=n->rect.x+n->rect.w/2;mouse_y=n->rect.y+n->rect.h/2;
        if(n->action<0)panel_mouse_queue(2,n->x,n->y,TIG_MESSAGE_MOUSE_MOVE);
        if(pad_focus_index!=before) {
            menu_hover_sound(n->action);
            if(!native_menu() || n->action<500 || n->action>=600)modern_pad_menu_pulse(&gamepad);
            if(pad_sheet() && n->action>=690 && n->action<700)sheet_select_detail(0,n->action-690);
            if(pad_sheet() && sheet_category==1 && n->action>=660 && n->action<664) {
                sheet_selected=sheet_page*4+n->action-660;sheet_select_detail(1,sheet_selected);
            }
            modern_pad_log("focus",pad_screen(),n->action);
            if(n->action<0)modern_pad_log("inventory-cell",n->x,n->y);
        }
    }
}

static void pad_activate_focus(bool secondary)
{
    if(pad_draw_context!=pad_screen())return;
    if(pad_cursor_mode) {
        /* Pointing is optional: trackpads/mouse can still reach maps and text. */
        int action=hit(mouse_x,mouse_y);
        if(action>=0){pending_action=action;return;}
        pad_focus_index=-1;
        for(int i=0;i<pad_focus_count;i++)if(inside(pad_focus_nodes[i].rect,mouse_x,mouse_y)) {pad_focus_index=i;break;}
        if(native_panel()==3) {
            int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
            SDL_Event point={0};point.type=SDL_EVENT_MOUSE_BUTTON_DOWN;point.button.button=SDL_BUTTON_LEFT;point.button.down=true;
            point.button.x=mouse_x*ww/canvas_w;point.button.y=mouse_y*wh/canvas_h;
            panels_event(&point);point.type=SDL_EVENT_MOUSE_BUTTON_UP;point.button.down=false;panels_event(&point);return;
        }
    }
    if(pad_focus_index<0 || pad_focus_index>=pad_focus_count)return;
    PadFocus* n=&pad_focus_nodes[pad_focus_index];
    tactical_pressed_actor=combat_input_actor();
    if(n->action>=0) {
        if(!secondary){
            int action=n->action;
            if(pad_value_pair(action)>=0 && !pad_action_available(action))action--;
            menu_button_sound(action,TIG_BUTTON_STATE_RELEASED);pending_action=action;modern_pad_log("activate",pad_screen(),action);
        }
    } else {
        int64_t item=modern_inventory_at(n->x,n->y);
        if(n->action==-2 && !inven_ui_drag_item_obj_get() && item && !inventory_match(item))return;
        modern_inventory_gamepad_activate(n->x,n->y,secondary);
    }
}

static void pad_cancel(void)
{
    modern_pad_log("cancel",pad_screen(),intgame_mode_get());
    if(pad_binding.type){pad_binding.type=HOTKEY_NONE;modern_pad_barrier(&gamepad);return;}
    if(pad_radial){pad_radial=false;pad_radial_slot=-1;modern_pad_barrier(&gamepad);return;}
    if(inven_ui_drag_item_obj_get()){sub_575770();return;}
    if(key_edit>=0){key_edit=-1;return;}
    if(!pad_modal()) {
        if(intgame_mode_get()==INTGAME_MODE_SPELL){spell_ui_cancel();return;}
        if(intgame_mode_get()==INTGAME_MODE_SKILL){skill_ui_cancel();return;}
        if(intgame_mode_get()==INTGAME_MODE_ITEM){item_ui_deactivate();return;}
        pad_target=0;modern_map_route_cancel_tracking();follow_walk_stop();modern_pad_barrier(&gamepad);
        return;
    }
    /* Reuse each screen's existing cancel route, without emitting keyboard events. */
    SDL_Event cancel={0};cancel.type=SDL_EVENT_KEY_DOWN;cancel.key.scancode=SDL_SCANCODE_ESCAPE;
    cancel.key.key=SDLK_ESCAPE;
    handle_ui_event(&cancel);
}

static bool pad_world_ready(void)
{
    return enabled && in_game && !pad_pause_request && !pad_modal() && !menu_suspended && !slide_ui_is_active()
        && !tig_movie_is_playing() && !modern_combat_input_blocked() && !SDL_TextInputActive(window)
        && SDL_GetKeyboardFocus()==window;
}

static bool pad_target_valid(int64_t actor)
{
    TigRect rect;
    return pad_target && obj_handle_is_valid(pad_target) && objid_is_equal(pad_target_id,obj_get_id(pad_target))
        && pad_target!=actor && health_actor_visible(pad_target,&rect) && !critter_is_dead(pad_target);
}

static void pad_cycle_target(void)
{
    int64_t actor=combat_input_actor();if(!actor)return;
    ObjectList list;object_list_vicinity(actor,OBJ_TM_CRITTER,&list);
    int64_t candidates[128];int count=0;
    for(ObjectNode* n=list.head;n && count<128;n=n->next) {
        int64_t obj=n->obj;TigRect rect;
        if(obj==actor || player_is_local_pc_obj(obj) || critter_pc_leader_get(obj)==player_get_local_pc_obj()
            || critter_is_dead(obj) || !health_actor_visible(obj,&rect))continue;
        if(combat_critter_is_combat_mode_active(actor) && !combat_critter_is_combat_mode_active(obj))continue;
        candidates[count++]=obj;
    }
    object_list_destroy(&list);
    /* Stable order prevents the linked-list order from changing the cycle. */
    for(int i=1;i<count;i++){int64_t value=candidates[i];int j=i;while(j>0 && candidates[j-1]>value){candidates[j]=candidates[j-1];j--;}candidates[j]=value;}
    int next=0;
    for(int i=0;i<count;i++)if(candidates[i]==pad_target)next=(i+1)%count;
    pad_target=count?candidates[next]:0;
    if(pad_target)pad_target_id=obj_get_id(pad_target);
    pad_cursor_mode=false;pad_target_stick_blocked=true;
    modern_pad_log("target-cycle",count,count?next:-1);
}

static bool pad_combat_preview(ModernCombatPreview* preview)
{
    return modern_combat_preview_at(preview,(int)(mouse_x*hrp_iso_window_width_get()/canvas_w),
        (int)(mouse_y*hrp_iso_window_height_get()/canvas_h),pad_target_valid(combat_input_actor())?pad_target:0);
}

static int64_t pad_front_target(int64_t actor)
{
    if(!actor || !obj_handle_is_valid(actor))return 0;
    int64_t from=obj_field_int64_get(actor,OBJ_F_LOCATION),ahead;
    int rotation=tig_art_id_rotation_get(obj_field_int32_get(actor,OBJ_F_CURRENT_AID));
    if(!location_in_dir(from,rotation,&ahead))return 0;
    float fx=LOCATION_GET_X(ahead)-LOCATION_GET_X(from),fy=LOCATION_GET_Y(ahead)-LOCATION_GET_Y(from);
    float best=1000;int64_t selected=0;
    ObjectList list;object_list_vicinity(actor,OBJ_TM_CRITTER|OBJ_TM_CONTAINER|OBJ_TM_PORTAL|OBJ_TM_ITEM|OBJ_TM_SCENERY,&list);
    for(ObjectNode* n=list.head;n;n=n->next) {
        int64_t obj=n->obj;
        if(obj_field_int32_get(obj,OBJ_F_TYPE)==OBJ_TYPE_SCENERY) {
            Script use;obj_arrayfield_script_get(obj,OBJ_F_SCRIPTS_IDX,SAP_USE,&use);
            if(!use.num)continue;
        }
        if(obj==actor || (obj_field_int32_get(obj,OBJ_F_FLAGS)&(OF_OFF|OF_DESTROYED|OF_EXTINCT|OF_DONTDRAW|OF_INVISIBLE|OF_CLICK_THROUGH)))continue;
        if(obj_field_int32_get(obj,OBJ_F_SPELL_FLAGS)&OSF_INVISIBLE)continue;
        if(obj_type_is_critter(obj_field_int32_get(obj,OBJ_F_TYPE)) && critter_is_concealed(obj))continue;
        int64_t tile=obj_field_int64_get(obj,OBJ_F_LOCATION);
        float dx=(int64_t)LOCATION_GET_X(tile)-(int64_t)LOCATION_GET_X(from),dy=(int64_t)LOCATION_GET_Y(tile)-(int64_t)LOCATION_GET_Y(from);
        float distance=dx*dx+dy*dy,dot=dx*fx+dy*fy,side=dx*fy-dy*fx;
        if(distance>9 || (distance>0 && (dot<=0 || dot*dot<0.5f*distance*(fx*fx+fy*fy))))continue;
        if(roof_is_covered_loc(tile,true))continue;
        TigRect rect;object_get_rect(obj,0,&rect);
        if(rect.width<=0 || rect.height<=0 || rect.x>=hrp_iso_window_width_get() || rect.y>=hrp_iso_window_height_get()
            || rect.x+rect.width<=0 || rect.y+rect.height<=0)continue;
        float score=distance+side*side;
        if(score>best || (score==best && selected && obj>selected))continue;
        /* Never select a nearby object that requires a detour around a wall. */
        AnimPath path;
        if(location_dist(from,tile)>1 && (!anim_preview_path(actor,tile,true,&path) || path.max>3))continue;
        best=score;selected=obj;
    }
    object_list_destroy(&list);return selected;
}

static void pad_world_activate(void)
{
    int64_t actor=combat_input_actor();
    if(!actor || !critter_is_active(actor))return;
    if(pad_cursor_mode && hud_region(mouse_x,mouse_y)) {
        int action=hit(mouse_x,mouse_y);
        if(action>=0){tactical_pressed_actor=actor;pending_action=action;}
        return;
    }
    if(!pad_cursor_mode && !pad_target_valid(actor) && intgame_mode_get()==INTGAME_MODE_MAIN) {
        int64_t object=pad_front_target(actor);
        if(!object || (tactical_active() && !combat_input_ready(actor))){modern_pad_log("interact-miss",intgame_mode_get(),0);return;}
        follow_walk_stop();modern_map_route_cancel_tracking();modern_pad_barrier(&gamepad);
        TargetDescriptor target={.obj=object,.is_loc=false};sub_54EA80(&target);
        modern_pad_log("interact-ahead",intgame_mode_get(),obj_field_int32_get(object,OBJ_F_TYPE));
        return;
    }
    if(tactical_active() && !combat_input_ready(actor))return;
    follow_walk_stop();modern_map_route_cancel_tracking();modern_pad_barrier(&gamepad);
    if(pad_target_valid(actor)) {
        TargetDescriptor target={.obj=pad_target,.is_loc=false};sub_54EA80(&target);
    } else modern_intgame_gamepad_point((int)(mouse_x*hrp_iso_window_width_get()/canvas_w),
        (int)(mouse_y*hrp_iso_window_height_get()/canvas_h));
}

static void pad_ui_tick(void)
{
    Uint64 now=SDL_GetTicks();
    float seconds=pad_tick_time?fminf((now-pad_tick_time)/1000.0f,0.05f):0;
    pad_tick_time=now;
    modern_pad_sample(&gamepad,SDL_GetKeyboardFocus()==window);
    int screen=pad_screen();
    if(screen!=pad_context || !enabled || (menu_suspended && !confirming_menu) || tig_movie_is_playing() || slide_ui_is_active()) {
        pad_context=screen;pad_radial=false;pad_radial_slot=-1;pad_repeat_direction=-1;pad_target=0;pad_cursor_mode=false;
        pad_target_stick_blocked=false;pad_binding.type=HOTKEY_NONE;modern_pad_barrier(&gamepad);return;
    }
    if(!gamepad.enabled || !gamepad.focused || !gamepad.device) {
        pad_radial=false;pad_target=0;pad_target_stick_blocked=false;pad_binding.type=HOTKEY_NONE;return;
    }
    Uint32 pressed=gamepad.pressed,released=gamepad.released;
    modern_pad_clear_edges(&gamepad);
    if(!gamepad.active){pad_radial=false;return;}
    if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_EAST)){pad_cancel();return;}
    if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_START)) {
        if(pad_modal())pad_cancel();
        else if(in_game) {
            pad_radial=false;pad_pause_request=true;pending_action=-1;
            follow_walk_stop();modern_pad_barrier(&gamepad);
            modern_pad_log("pause-request",pad_screen(),intgame_mode_get());
        }
        return;
    }
    bool modal=pad_modal();
    static int navigation=-1;
    int navigation_now=(native_panel()==3 && gamepad.left.length>0.05f?1:0)
        |(gamepad.left_trigger?2:0)|(gamepad.right_trigger?4:0);
    if(navigation_now!=navigation){modern_pad_log("navigation",screen,navigation_now);navigation=navigation_now;}
    int mode=intgame_mode_get();
    if(mode!=pad_target_mode) {
        pad_target_mode=mode;
        if(pad_aiming()) {
            pad_cursor_mode=true;pad_target=0;mouse_x=canvas_w/2;mouse_y=canvas_h/2;
            modern_pad_log("aim",screen,mode);
        }
    }
    /* Leaving combat is not an actor action: native threat checks still apply. */
    if(!modal && in_game && !menu_suspended && !slide_ui_is_active()
        && (pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_NORTH))) {
        modern_intgame_gamepad_combat();modern_pad_log("combat-toggle",screen,combat_turn_based_is_active());return;
    }
    if(pad_binding.type) {
        int sector=modern_pad_sector(gamepad.right.length>0.35f?gamepad.right:gamepad.left,10);
        if(sector>=0)pad_binding_slot=sector;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))pad_binding_slot=(pad_binding_slot+1)%10;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_LEFT))pad_binding_slot=(pad_binding_slot+9)%10;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_SOUTH)) {
            pad_binding.slot=pad_binding_slot+(gamepad.left_trigger?10:0);binding_pending=pad_binding;
            modern_pad_log("bind",pad_binding.type,pad_binding.slot);
            pad_binding.type=HOTKEY_NONE;modern_pad_barrier(&gamepad);
        }
        return;
    }
    if(modal && (native_panel()==2 || native_panel()==5) && (pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_WEST))) {
        if(!pad_cursor_mode && pad_focus_index>=0) {
            SDL_FRect r=pad_focus_nodes[pad_focus_index].rect;mouse_x=r.x+r.w/2;mouse_y=r.y+r.h/2;
        }
        pad_binding=binding_hover();pad_binding_slot=0;
        int64_t carried=inven_ui_drag_item_obj_get(),owner;
        if(carried && item_parent(carried,&owner) && owner==combat_input_actor())
            pad_binding=(QuickBinding){.type=HOTKEY_ITEM,.item=carried,.actor=owner};
        if(pad_binding.type){modern_pad_log("bind-open",screen,pad_binding.type);modern_pad_barrier(&gamepad);return;}
    }
    if(!modal && pad_world_ready()) {
        if(!pad_radial && (pressed&(PAD_BUTTON(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)|PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)))) {
            pad_menu_radial=(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))!=0;
            pad_radial=true;pad_radial_slot=-1;pad_radial_actor=combat_input_actor();pad_radial_offset=0;
            follow_walk_stop();
        }
        if(pad_radial) {
            pad_radial_offset=!pad_menu_radial && gamepad.left_trigger?10:0;
            int slot=modern_pad_sector(gamepad.right.length>0.35f?gamepad.right:gamepad.left,pad_menu_radial?9:10);
            if(slot>=0)pad_radial_slot=slot;
            if(released&PAD_BUTTON(pad_menu_radial?SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)) {
                pad_radial=false;
                if(pad_radial_slot>=0 && pad_radial_actor==combat_input_actor()) {
                    tactical_pressed_actor=pad_radial_actor;
                    pending_action=pad_menu_radial?pad_menu_actions[pad_radial_slot]:300+pad_radial_offset+pad_radial_slot;
                    if(tactical_active() && pending_action==1)pending_action=1105;
                    if(tactical_active() && pending_action==2)pending_action=1104;
                    modern_pad_log("wheel-action",pad_menu_radial,pending_action);
                }
                modern_pad_barrier(&gamepad);
            }
            return;
        }
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_STICK))pad_cycle_target();
        if(!tactical_active() && !pad_aiming() && gamepad.left.length>0.1f) {
            pad_cursor_mode=false;
            if(!combat_critter_is_combat_mode_active(combat_input_actor()))pad_target=0;
        }
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_SOUTH))pad_world_activate();
        tactical_pressed_actor=combat_input_actor();
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_WEST))pending_action=tactical_active()?1104:2;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_BACK))pending_action=0;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_LEFT_STICK) && tactical_active())pending_action=1100;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_UP))pending_action=0;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_LEFT))pending_action=tactical_active()?1105:1;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))pending_action=3;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_DOWN))pending_action=tactical_active()?1104:2;
    }
    if(modal) {
        int dx=0,dy=0;
        Uint32 directions=gamepad.held|pressed;
        if(directions&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_LEFT))dx=-1;
        if(directions&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_RIGHT))dx=1;
        if(directions&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_UP))dy=-1;
        if(directions&PAD_BUTTON(SDL_GAMEPAD_BUTTON_DPAD_DOWN))dy=1;
        if(native_panel()==3 && memo_edit<0 && gamepad.left.length>0.05f) {
            map_pan_x+=gamepad.left.x*seconds*700;map_pan_y+=gamepad.left.y*seconds*700;
        }
        if(!dx && !dy && native_panel()!=3 && gamepad.left.length>0.45f) {
            if(fabsf(gamepad.left.x)>fabsf(gamepad.left.y))dx=gamepad.left.x<0?-1:1;
            else dy=gamepad.left.y<0?-1:1;
        }
        int direction=dx || dy?(dy+1)*3+dx+1:-1;
        if(direction>=0 && (direction!=pad_repeat_direction || now>=pad_repeat_at)) {
            pad_move_focus(dx,dy);pad_repeat_at=now+(direction!=pad_repeat_direction?350:110);
        }
        pad_repeat_direction=direction;
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_SOUTH))pad_activate_focus(false);
        if(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_NORTH) && native_panel()==2)pad_activate_focus(true);
        if(pressed&(PAD_BUTTON(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)|PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))) {
            bool next=(pressed&PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))!=0;
            SDL_Event page={0};page.type=SDL_EVENT_KEY_DOWN;page.key.scancode=next?SDL_SCANCODE_PAGEDOWN:SDL_SCANCODE_PAGEUP;
            if(native_menu() && mainmenu_ui_window_type_get()==MM_WINDOW_OPTIONS && options_tab==3) {
                if(key_gamepad)pad_options_page=next?1:0;else key_options_action(next?420:419);
            } else if(pad_sheet()) {
                int action=pad_focus_index>=0?pad_focus_nodes[pad_focus_index].action:-1;
                int section=action>=690 && action<696?0:action>=696 && action<700?1:2;
                const int starts[]={692,696,651};int desired=starts[(section+(next?1:2))%3];
                for(int i=0;i<pad_focus_count;i++)if(pad_focus_nodes[i].action==desired) {
                    pad_focus_index=i;pad_cursor_mode=false;modern_pad_menu_pulse(&gamepad);
                    modern_pad_log("sheet-section",section,desired);break;
                }
            } else if(native_panel()==2)modern_inventory_scroll(next?1:-1);
            else handle_ui_event(&page);
        }
    }
    /* Clicking the stick often deflects it. Require release and neutral before
     * pointer motion may replace a deliberately selected actor. */
    if(pad_target_stick_blocked && gamepad.right.length<=0.05f
        && !(gamepad.held&PAD_BUTTON(SDL_GAMEPAD_BUTTON_RIGHT_STICK)))pad_target_stick_blocked=false;
    if(!pad_target_stick_blocked && gamepad.right.length>0.05f) {
        pad_cursor_mode=true;pad_target=0;
        mouse_x=SDL_clamp(mouse_x+gamepad.right.x*seconds*800*gamepad.cursor_speed/100,0,canvas_w-1);
        mouse_y=SDL_clamp(mouse_y+gamepad.right.y*seconds*800*gamepad.cursor_speed/100,0,canvas_h-1);
    }
    if(!pad_radial && (gamepad.left_trigger || gamepad.right_trigger)) {
        float amount=(gamepad.right_trigger?1.0f:0)-(gamepad.left_trigger?1.0f:0);
        if(native_panel()==3)map_zoom(powf(2,amount*seconds),map_destination.x+map_destination.w/2,map_destination.y+map_destination.h/2);
        else if(!modal && world_zoom_allowed()) {
            float minimum=fmaxf(.5f,fmaxf(zoom_base_width/4096.0f,zoom_base_height/4096.0f));
            float maximum=fminf(zoom_base_width/800.0f,zoom_base_height/600.0f);
            world_zoom=SDL_clamp(world_zoom*powf(2,amount*seconds),minimum,maximum);
            zoom_anchor_x=zoom_anchor_y=.5f;zoom_pending=true;
        }
    }
    if(!modal && pad_cursor_mode && !hud_region(mouse_x,mouse_y))
        modern_intgame_gamepad_hover((int)(mouse_x*hrp_iso_window_width_get()/canvas_w),(int)(mouse_y*hrp_iso_window_height_get()/canvas_h));
}

static void pad_glyph(SDL_FRect r,SDL_GamepadButton button)
{
    outline(r,GOLD);
    SDL_GamepadButtonLabel label=gamepad.device?SDL_GetGamepadButtonLabel(gamepad.device,button):SDL_GAMEPAD_BUTTON_LABEL_UNKNOWN;
    float cx=r.x+r.w/2,cy=r.y+r.h/2;
    SDL_SetRenderDrawColor(renderer,GOLD.r,GOLD.g,GOLD.b,255);
    if(label==SDL_GAMEPAD_BUTTON_LABEL_CROSS) {
        SDL_RenderLine(renderer,cx-6,cy-6,cx+6,cy+6);SDL_RenderLine(renderer,cx+6,cy-6,cx-6,cy+6);
    } else if(label==SDL_GAMEPAD_BUTTON_LABEL_SQUARE)outline((SDL_FRect){cx-6,cy-6,12,12},GOLD);
    else if(label==SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE) {
        SDL_FPoint p[]={{cx,cy-7},{cx+7,cy+6},{cx-7,cy+6},{cx,cy-7}};SDL_RenderLines(renderer,p,4);
    } else if(label==SDL_GAMEPAD_BUTTON_LABEL_CIRCLE) {
        SDL_FPoint p[25];for(int i=0;i<=24;i++){float a=i*6.2831853f/24;p[i]=(SDL_FPoint){cx+7*cosf(a),cy+7*sinf(a)};}SDL_RenderLines(renderer,p,25);
    } else button_label(0,modern_pad_button_name(&gamepad,button),r,true,GOLD);
}

static void pad_wheel_label(const char* label,SDL_FRect rect)
{
    ButtonTextLayout layout=button_text_layout(BUTTON_FONT,label,rect.w,rect.h);
    float gap=6*text_scale/100.0f;
    float height=layout.ink[0].h+(layout.count==2?gap+layout.ink[1].h:0);
    float y=rect.y+(rect.h-height)/2;
    for(int i=0;i<layout.count;i++) {
        SDL_FRect ink=layout.ink[i];
        text_draw(BUTTON_FONT,layout.lines[i],rect.x+(rect.w-ink.w)/2-ink.x,y-ink.y,GOLD);
        y+=ink.h+gap;
    }
}

static void pad_draw(void)
{
    pad_collect_focus();
    if(!gamepad.active || !gamepad.enabled || !gamepad.device)return;
    if(pad_modal() && !pad_cursor_mode && pad_focus_index>=0) {
        SDL_FRect r=pad_focus_nodes[pad_focus_index].rect;
        r.x-=3;r.y-=3;r.w+=6;r.h+=6;outline(r,(SDL_Color){255,227,151,255});
        r.x-=1;r.y-=1;r.w+=2;r.h+=2;outline(r,GOLD);
    }
    if(!pad_modal() && pad_target_valid(combat_input_actor())) {
        TigRect target;object_get_rect(pad_target,0,&target);
        float sx=canvas_w/hrp_iso_window_width_get(),sy=canvas_h/hrp_iso_window_height_get();
        outline((SDL_FRect){target.x*sx-4,target.y*sy-4,target.width*sx+8,target.height*sy+8},GOLD);
    }
    if(pad_radial || pad_binding.type) {
        float cx=canvas_w/2,cy=canvas_h/2;
        bool menus=pad_radial && pad_menu_radial;
        int selected=pad_binding.type?pad_binding_slot:pad_radial_slot;
        int offset=pad_binding.type?(gamepad.left_trigger?10:0):pad_radial_offset;
        float half_width=menus?410:280,half_height=menus?340:280;
        fill((SDL_FRect){cx-half_width,cy-half_height,half_width*2,half_height*2},(SDL_Color){12,9,7,245});
        int count=menus?9:10;
        for(int i=0;i<count;i++) {
            float angle=i*6.2831853f/count-1.5707963f;
            float width=menus?180:98;
            float height=menus?96:72;
            SDL_FRect r={cx+(menus?310:215)*cosf(angle)-width/2,cy+(menus?290:215)*sinf(angle)-height/2,width,height};
            fill(r,i==selected?(SDL_Color){96,64,27,255}:(SDL_Color){34,27,19,255});outline(r,i==selected?GOLD:MUTED);
            if(menus)pad_wheel_label(game_text(modern_intgame_text(pad_menu_labels[i])),(SDL_FRect){r.x+5,r.y+8,r.w-10,r.h-16});
            else {
                Hotkey* hotkey=modern_hotkey_get(offset+i);
                if(hotkey && hotkey->type!=HOTKEY_NONE)draw_art_id(hotkey->art_id,(SDL_FRect){r.x+28,r.y+7,42,42},true);
                char number[16];snprintf(number,sizeof(number),"%s%d",offset?"Alt + ":"",(i+1)%10);button_label(0,number,(SDL_FRect){r.x,r.y+47,r.w,24},true,GOLD);
            }
        }
        button_label(1,pad_binding.type?tr("A : affecter","A: assign"):menus?tr("Relâcher RB / R1","Release RB / R1"):tr("Relâcher LB / L1","Release LB / L1"),
            (SDL_FRect){cx-145,cy-65,290,50},true,GOLD);
        if(!menus)button_label(0,offset?tr("Barre Alt · LT maintenu","Alt bar · holding LT"):tr("LT : barre Alt","LT: Alt bar"),
            (SDL_FRect){cx-145,cy-15,290,35},true,MUTED);
        pad_glyph((SDL_FRect){cx-55,cy+18,28,28},SDL_GAMEPAD_BUTTON_EAST);
        button_label(0,tr("Annuler","Cancel"),(SDL_FRect){cx-18,cy+18,100,28},false,MUTED);
        return;
    }
    bool modal=pad_modal();
    bool binding=native_panel()==2 || native_panel()==5;
    bool combat=!modal && combat_critter_is_combat_mode_active(player_get_local_pc_obj());
    float width=modal?((pad_options() || pad_sheet())?fminf(1160,canvas_w-24):binding?740:native_panel()==3?980:360):fminf(1200,canvas_w-24);
    float x=(canvas_w-width)/2;
    /* The tactical maintained-spells panel occupies y=82..146. */
    float y=modal?8:tactical_active()?154:top_bar_rect(0,0,0,TOP_BAR_SOURCE_HEIGHT).h+8;
    SDL_FRect strip={x,y,width,modal?40:76};if(modal)fill(strip,(SDL_Color){12,9,7,220});
    float first=modal?x:x+(width-(combat?980:360))/2;
    pad_glyph((SDL_FRect){first+8,y+6,28,28},SDL_GAMEPAD_BUTTON_SOUTH);
    button_label(0,modal?tr("Valider","Confirm"):pad_aiming()?tr("Viser","Target"):combat?tr("Attaquer","Attack"):tr("Interagir","Interact"),(SDL_FRect){first+44,y+2,125,36},true,GOLD);
    pad_glyph((SDL_FRect){first+188,y+6,28,28},SDL_GAMEPAD_BUTTON_EAST);
    button_label(0,tr("Retour","Back"),(SDL_FRect){first+224,y+2,125,36},true,GOLD);
    if(combat) {
        pad_glyph((SDL_FRect){first+368,y+6,28,28},SDL_GAMEPAD_BUTTON_NORTH);
        button_label(0,tr("Quitter le combat","Leave combat"),(SDL_FRect){first+404,y+2,208,36},true,GOLD);
        pad_glyph((SDL_FRect){first+638,y+6,40,28},SDL_GAMEPAD_BUTTON_RIGHT_STICK);
        button_label(0,tr("Cible suivante","Next target"),(SDL_FRect){first+686,y+2,286,36},true,GOLD);
    }
    if(pad_sheet()) {
        button_label(0,tr("Haut/bas : ligne · Gauche/droite : valeur · LB/RB : zone","Up/down: row · Left/right: value · LB/RB: section"),
            (SDL_FRect){x+365,y+2,width-373,36},true,MUTED);
    } else if(pad_options()) {
        button_label(0,tr("Haut / bas : ligne · Gauche / droite : régler","Up / down: row · Left / right: adjust"),
            (SDL_FRect){x+365,y+2,width-373,36},true,MUTED);
    } else if(binding) {
        pad_glyph((SDL_FRect){x+368,y+6,28,28},SDL_GAMEPAD_BUTTON_WEST);
        button_label(0,tr("Affecter un raccourci","Assign quick slot"),(SDL_FRect){x+404,y+2,width-412,36},true,GOLD);
    } else if(native_panel()==3)button_label(0,tr("Stick gauche : déplacer · LT / RT : zoom","Left stick: pan · LT / RT: zoom"),(SDL_FRect){x+365,y+2,width-373,36},true,MUTED);
    if(!modal)button_label(0,tr("LB : raccourcis · RB : menus · LT / RT : zoom · Start : pause","LB: quick slots · RB: menus · LT / RT: zoom · Start: pause"),
        (SDL_FRect){x+8,y+38,width-16,34},true,MUTED);
}
