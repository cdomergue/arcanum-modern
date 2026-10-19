/* Party portraits use the native-resolution canvas, never the world raster. */
static int64_t party_members[128],party_pressed_obj,party_action_obj,party_menu_obj;
static int party_count,party_page,party_pressed=-1;
static bool party_capture;
static Uint8 party_pressed_button;
static float party_menu_y;
static bool party_visible(void)
{
    return enabled && hud && visible() && !native_menu() && !native_panel() && !journal;
}
static int party_rows(void){return (int)fmaxf(1,fminf(6,(canvas_h-236)/108));}
static SDL_FRect party_rect(int row){return (SDL_FRect){12,88+row*108,94,106};}
static SDL_FRect party_menu_rect(void){return (SDL_FRect){114,party_menu_y,265,294};}
static bool party_region(float x,float y)
{
    if(!party_visible())return false;
    int count=party_count-party_page*party_rows();if(count>party_rows())count=party_rows();
    return (count>0 && inside((SDL_FRect){12,88,94,count*108+(party_count>party_rows()?28:0)+(modern_party_selected()?30:0)},x,y))
        || (party_menu_obj && inside(party_menu_rect(),x,y));
}
static void party_tick(void)
{
    modern_followers_sync(enabled);
    if(!party_visible()){party_menu_obj=0;return;}
    party_count=modern_followers_get(party_members,128);
    int pages=(party_count+party_rows()-1)/party_rows();
    if(party_page>=pages)party_page=pages>0?pages-1:0;
    if(party_menu_obj){
        bool found=false;for(int i=0;i<party_count;i++)if(party_members[i]==party_menu_obj)found=true;
        if(!found)party_menu_obj=0;
    }
}
static void draw_party(void)
{
    if(!party_visible())return;
    for(int row=0;row<party_rows();row++){
        int index=party_page*party_rows()+row;if(index>=party_count)break;
        int64_t obj=party_members[index];if(!obj_handle_is_valid(obj))continue;
        SDL_FRect r=party_rect(row);
        button(r,1800+index,"",-1,(tactical_active() && obj==tactical_actor) || obj==modern_party_selected(),true);
        if(!tactical_active())button((SDL_FRect){r.x+4,r.y+4,22,22},2600+index,modern_party_detached(obj)?"/":"=",-1,false,intgame_mode_get()==INTGAME_MODE_MAIN);
        tactical_portrait(obj,(SDL_FRect){r.x+29,r.y+5,36,46});
        char name[1024];object_examine(obj,player_get_local_pc_obj(),name);
        text_compact(game_text(name),r.x+4,r.y+52,r.w-8,true,GOLD);
        int hp=object_hp_current(obj),max=object_hp_max(obj);
        int ft=critter_fatigue_current(obj),ftmax=critter_fatigue_max(obj);
        SDL_Color health=stat_level_get(obj,STAT_POISON_LEVEL)>=20?(SDL_Color){90,174,75,255}:(SDL_Color){192,54,42,255};
        fill((SDL_FRect){r.x+6,r.y+74,82,5},(SDL_Color){49,30,25,255});
        fill((SDL_FRect){r.x+6,r.y+74,82*fmaxf(0,fminf(1,max>0?(float)hp/max:0)),5},health);
        fill((SDL_FRect){r.x+6,r.y+82,82,4},(SDL_Color){29,30,49,255});
        fill((SDL_FRect){r.x+6,r.y+82,82*fmaxf(0,fminf(1,ftmax>0?(float)ft/ftmax:0)),4},(SDL_Color){83,113,213,255});
        char values[60];snprintf(values,sizeof(values),"%d / %d",hp,max);
        text_compact(values,r.x+4,r.y+87,r.w-8,true,GOLD);
        if(inside(r,mouse_x,mouse_y) && !party_menu_obj){
            char detail[1200];snprintf(detail,sizeof(detail),"%s  ·  %s : %d / %d  ·  %s : %d / %d%s%s",game_text(name),character_text(106),hp,max,character_text(107),ft,ftmax,tactical_active()?"":"  ·  ",tactical_active()?"":modern_party_detached(obj)?tr("Détaché — cliquer / pour regrouper","Ungrouped — click / to group"):tr("Groupé — cliquer = pour détacher","Grouped — click = to ungroup"));
            float width=fminf(canvas_w-138,text_measure(0,detail)+24);
            SDL_FRect tip={114,r.y+15,width,31};fill(tip,(SDL_Color){16,12,9,235});outline(tip,GOLD);
            text_compact(detail,tip.x+12,tip.y+6,tip.w-24,false,GOLD);
        }
    }
    if(party_count>party_rows()){
        float y=88+party_rows()*108;
        button((SDL_FRect){12,y,43,27},1930,"<",-1,false,party_page>0);
        button((SDL_FRect){63,y,43,27},1931,">",-1,false,(party_page+1)*party_rows()<party_count);
    }
    if(modern_party_selected()){
        int rows=party_count-party_page*party_rows();if(rows>party_rows())rows=party_rows();
        button((SDL_FRect){12,88+rows*108+(party_count>party_rows()?28:0),94,28},2730,tr("Personnage","Player"),-1,false,true);
    }
    if(party_menu_obj){
        SDL_FRect r=party_menu_rect();frame(r);
        for(int i=0;i<8;i++)tactical_button((SDL_FRect){r.x+8,r.y+8+i*35,r.w-16,31},1940+i,game_text(modern_follower_text(i)),false,modern_follower_command_available(party_menu_obj,i));
    }
}
static bool party_action(int action)
{
    if(action==2730){modern_party_select(0);location_origin_set(obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION));return true;}
    if(action>=2600 && action<2728){
        modern_party_set_grouped(party_action_obj,modern_party_detached(party_action_obj));return true;
    }
    if(action==1930 || action==1931){party_page+=action==1930?-1:1;if(party_page<0)party_page=0;return true;}
    if(action>=1940 && action<1948){modern_follower_command(party_action_obj,action-1940);return true;}
    if(action<1800 || action>=1928)return false;
    int64_t obj=party_action_obj;
    if(!obj || !obj_handle_is_valid(obj) || !player_is_local_pc_obj(critter_pc_leader_get(obj)))return true;
    if(intgame_mode_get()==INTGAME_MODE_DIALOG)return true;
    if(tactical_active() && intgame_mode_get()==INTGAME_MODE_MAIN){
        if(!combat_shared_turn_select(obj))tactical_camera_focus(obj,false);return true;
    }
    if(!tactical_active() && intgame_mode_get()==INTGAME_MODE_MAIN){
        if(modern_party_selected()==obj)modern_party_select(0);
        else if(modern_party_select(obj))location_origin_set(obj_field_int64_get(obj,OBJ_F_LOCATION));
        return true;
    }
    if(tactical_active() && !combat_input_ready(combat_input_actor()))return true;
    TargetDescriptor td;target_descriptor_set_obj(&td,obj);sub_54EA80(&td);
    return true;
}
static bool party_event(SDL_Event* event)
{
    if(!party_visible()){party_menu_obj=0;party_capture=false;return false;}
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);if(ww<=0 || wh<=0)return false;
    if(party_menu_obj && (event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) && event->key.scancode==SDL_SCANCODE_ESCAPE){
        if(event->type==SDL_EVENT_KEY_DOWN)party_menu_obj=0;return true;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION)return party_region(event->motion.x*canvas_w/ww,event->motion.y*canvas_h/wh);
    if(event->type==SDL_EVENT_MOUSE_WHEEL){
        if(!party_region(event->wheel.mouse_x*canvas_w/ww,event->wheel.mouse_y*canvas_h/wh))return false;
        if(!party_menu_obj){float delta=event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event->wheel.y:event->wheel.y;
            if(delta>0 && party_page>0)party_page--;else if(delta<0 && (party_page+1)*party_rows()<party_count)party_page++;}
        return true;
    }
    if(event->type!=SDL_EVENT_MOUSE_BUTTON_DOWN && event->type!=SDL_EVENT_MOUSE_BUTTON_UP)return false;
    float x=event->button.x*canvas_w/ww,y=event->button.y*canvas_h/wh;
    if(event->button.down){
        if(!party_menu_obj && !party_region(x,y))return false;
        party_capture=true;party_pressed=hit(x,y);party_pressed_button=event->button.button;
        party_pressed_obj=party_pressed>=1800 && party_pressed<1800+party_count?party_members[party_pressed-1800]:party_pressed>=2600 && party_pressed<2600+party_count?party_members[party_pressed-2600]:party_menu_obj;
        if(party_menu_obj && !inside(party_menu_rect(),x,y)){party_menu_obj=0;party_pressed=-1;}
        return true;
    }
    if(!party_capture)return false;
    party_capture=false;
    if(event->button.button!=party_pressed_button || party_pressed!=hit(x,y))return true;
    if(party_pressed_button==SDL_BUTTON_RIGHT && party_pressed>=1800 && party_pressed<1928){
        party_menu_obj=party_pressed_obj;party_menu_y=fminf(canvas_h-430,fmaxf(88,y));
    } else if(party_pressed_button==SDL_BUTTON_LEFT && party_pressed>=1800){
        party_action_obj=party_pressed_obj;pending_action=party_pressed;party_menu_obj=0;
    }
    return true;
}
