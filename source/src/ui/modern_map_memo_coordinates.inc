static bool memo_coordinate_value(const char* text,int* value)
{
    if(!*text)return false;
    for(const char* p=text;*p;p++)if(*p<'0' || *p>'9')return false;
    char* end;errno=0;long parsed=strtol(text,&end,10);
    if(errno || *end || parsed<0 || parsed>INT_MAX)return false;
    *value=(int)parsed;return true;
}
static bool memo_coordinate_position(int* x,int* y)
{
    int west,south;
    return memo_coordinate_value(memo_west,&west) && memo_coordinate_value(memo_south,&south)
        && modern_map_coords_point(west,south,x,y);
}
static bool memo_coordinate_action(int action)
{
    if(action==2195) {
        if(map_memo_count>=64 || memo_map_state.moving)return true;
        memo_draft=(MapMemo){.mode=memo_map_state.mode,.map=memo_map_state.map,.townmap=memo_map_state.townmap};
        memo_placing=false;memo_focus(64);memo_coords_changed=true;memo_field=1;memo_select_all=true;
        int west,south;
        if(modern_map_position_coords(&west,&south)) {
            snprintf(memo_west,sizeof(memo_west),"%d",west);snprintf(memo_south,sizeof(memo_south),"%d",south);
        }
        return true;
    }
    if(memo_edit>=0 && action>=2196 && action<=2198) {
        memo_field=action-2196;memo_select_all=true;return true;
    }
    return false;
}
static void draw_memo_field(SDL_FRect r,int field,const char* value,const char* placeholder)
{
    fill(r,(SDL_Color){19,14,10,255});outline(r,memo_field==field?UI_RED_BORDER:(SDL_Color){113,85,47,255});
    if(memo_field==field && memo_select_all && *value)
        fill((SDL_FRect){r.x+5,r.y+5,r.w-10,r.h-10},(SDL_Color){90,61,28,150});
    if(field)button_label(0,*value?value:placeholder,(SDL_FRect){r.x+12,r.y+3,r.w-24,r.h-6},false,*value?GOLD:MUTED);
    else menu_paragraph(*value?value:placeholder,(SDL_FRect){r.x+12,r.y+6,r.w-24,r.h-12},0,*value?GOLD:MUTED);
    if(memo_field==field && !memo_select_all && (field || !*value) && (SDL_GetTicks()/450)%2==0)
        fill((SDL_FRect){r.x+12+fminf(text_measure(0,value),r.w-26),r.y+8,2,fminf(text_line_height(),r.h-16)},GOLD);
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){r,2196+field,true};
}
static void draw_memo_menu(SDL_FRect anchor)
{
    if(!memo_menu_open || memo_edit>=0)return;
    const char* click=tr("Par clic","By click");
    const char* coords=tr("Par coordonnées","By coordinates");
    float width=fmaxf(anchor.w,ceilf(button_text_bounds(BUTTON_FONT,coords).w)+48);
    SDL_FRect menu={fminf(anchor.x,canvas_w-width-12),anchor.y+anchor.h+4,width,116};
    hud_panel(menu);button_overflow_hint[0]=0;
    button((SDL_FRect){menu.x+8,menu.y+8,width-16,48},2190,click,-1,false,true);
    button((SDL_FRect){menu.x+8,menu.y+60,width-16,48},2195,coords,-1,false,true);
}
static bool memo_menu_event(SDL_Event* event)
{
    if(!memo_menu_open)return false;
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        if(event->type==SDL_EVENT_KEY_DOWN && event->key.scancode==SDL_SCANCODE_ESCAPE)
            memo_menu_open=false;
        return true;
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        int action=hit(mouse_x,mouse_y);
        bool choice=event->button.button==SDL_BUTTON_LEFT && (action==2190 || action==2195 || action==2199);
        if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN)pressed_action=choice?action:-1;
        else {
            if(choice && pressed_action==action && action!=2199)pending_action=action;
            memo_menu_open=false;pressed_action=-1;click_captured=false;
        }
        /* Consume the whole click, including dismissal outside the menu:
         * it must not place a route point or a note on the map underneath. */
        return true;
    }
    return event->type==SDL_EVENT_MOUSE_MOTION || event->type==SDL_EVENT_MOUSE_WHEEL
        || event->type==SDL_EVENT_TEXT_INPUT;
}
