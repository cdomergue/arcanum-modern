/* Tactical presentation. Order, PA, rolls and execution belong to the engine. */
static int64_t tactical_actor, tactical_pressed_actor;
static int64_t tactical_order[128];
static int tactical_count, tactical_page, tactical_spell_page;
static int tactical_maintained_ids[32],tactical_maintained_spells[32],tactical_maintained_count;
static bool tactical_was_active, tactical_log_open;
static ModernCombatPreview tactical_preview;
static bool tactical_has_preview;
static Uint64 tactical_preview_time;
static char tactical_lines[32][1200];
static int tactical_line_count;
static struct { int number; SDL_Texture* texture; float width,height; } tactical_portraits[32];
static int tactical_portrait_count;

/* Rendering can occur inside engine transitions, before the next UI tick.
 * A turn-order snapshot must never keep a freed object alive implicitly. */
static int64_t tactical_order_actor(int index)
{
    if(index<0 || index>=tactical_count || index>=128)return OBJ_HANDLE_NULL;
    int64_t actor=tactical_order[index];
    return actor && obj_handle_is_valid(actor)?actor:OBJ_HANDLE_NULL;
}

/* Only newly introduced controls need new strings. All game names, statistics,
 * spells, costs and outcomes below come from the installed localization. */
enum {TC_END,TC_AP,TC_ROUND,TC_WAIT,TC_MOVE,TC_ATTACK,TC_BLOCKED,TC_RANGE,TC_FATIGUE,TC_BUSY,TC_LOG,TC_AFTER_MOVE,TC_MAINTAINED};
static const char* tactical_text(int id)
{
    static const char* en[]={"End turn","AP","Round","Enemy turn","Move","Attack","No path / blocked","Out of range","Fatigue","Action in progress","Combat log","After approach","Maintained spells"};
    static const char* fr[]={"Fin du tour","PA","Tour","Tour adverse","Déplacement","Attaque","Chemin bloqué","Hors de portée","Fatigue","Action en cours","Journal de combat","Après approche","Sorts maintenus"};
    return strcmp(modern_intgame_text(0),"Niveau")==0?fr[id]:en[id];
}

void modern_combat_log(int64_t object,const char* raw)
{
    if(!enabled || !combat_turn_based_is_active() || !raw || !*raw || !obj_handle_is_valid(object))return;
    char name[1000],text[1000];object_examine(object,player_get_local_pc_obj(),name);
    from_game(raw,text,sizeof(text));
    char line[1200];snprintf(line,sizeof(line),"%s : %.900s",game_text(name),text);
    SDL_strlcpy(tactical_lines[tactical_line_count++%32],line,sizeof(tactical_lines[0]));
}

static void tactical_portraits_clear(void)
{
    for(int i=0;i<tactical_portrait_count;i++)SDL_DestroyTexture(tactical_portraits[i].texture);
    tactical_portrait_count=0;
}
static void tactical_portrait(int64_t actor,SDL_FRect rect)
{
    int64_t viewer=player_get_local_pc_obj();
    if(!actor || !obj_handle_is_valid(actor) || !viewer || !obj_handle_is_valid(viewer))return;
    int number=-1;bool portrait=intgame_examine_portrait(viewer,actor,&number);
    if(!portrait){draw_art(number,rect,true);return;}
    int i=0;while(i<tactical_portrait_count && tactical_portraits[i].number!=number)i++;
    if(i==tactical_portrait_count && i<32){
        tactical_portrait_count++;tactical_portraits[i].number=number;tactical_portraits[i].texture=NULL;
        char path[260];portrait_path(number,path,sizeof(path),64);TigVideoBuffer* buffer=NULL;
        if(*path && tig_video_buffer_load_from_bmp(path,&buffer,1)==TIG_OK){
            tactical_portraits[i].texture=texture_from_buffer(buffer,&tactical_portraits[i].width,&tactical_portraits[i].height);
            tig_video_buffer_destroy(buffer);
        }
    }
    if(i<32)fitted_texture(tactical_portraits[i].texture,tactical_portraits[i].width,tactical_portraits[i].height,rect);
}

static bool tactical_active(void) {return combat_tactical_enabled() && combat_turn_based_is_active();}
static struct {
    bool moving;
    int64_t target, x, y, dx, dy;
    Uint64 started, unlock;
    Uint32 buttons;
    bool keys[SDL_SCANCODE_COUNT];
} tactical_camera;

bool modern_combat_input_blocked(void)
{
    if(!enabled || !tactical_active() || mainmenu_ui_is_active())return false;
    int64_t actor=combat_turn_based_whos_turn_get();
    return (actor!=tactical_actor && combat_player_controls(actor))
        || tactical_camera.moving || SDL_GetTicks()<tactical_camera.unlock;
}

static void tactical_camera_focus(int64_t actor,bool notify)
{
    if(!actor || !obj_handle_is_valid(actor))return;
    tactical_camera.target=actor;
    location_origin_get(&tactical_camera.x,&tactical_camera.y);
    location_calc_dist_from_screen_center(obj_field_int64_get(actor,OBJ_F_LOCATION),&tactical_camera.dx,&tactical_camera.dy);
    tactical_camera.started=SDL_GetTicks();
    tactical_camera.moving=true;
    tactical_camera.unlock=0;
    tactical_camera.buttons|=SDL_GetMouseState(NULL,NULL);
    const bool* keys=SDL_GetKeyboardState(NULL);
    for(int key=1;key<SDL_SCANCODE_COUNT;key++)
        if(key!=SDL_SCANCODE_ESCAPE && keys[key])tactical_camera.keys[key]=true;
    scroll_stop();
    for(int button=TIG_MOUSE_BUTTON_LEFT;button<=TIG_MOUSE_BUTTON_MIDDLE;button++)tig_mouse_cancel_button(button);
    pending_action=pressed_action=-1;click_captured=false;tactical_has_preview=false;
    if(notify){
        tig_sound_handle_t sound=gsound_play_sfx(SND_INTERFACE_BUTTON_LOW_RELEASE,1);
        if(sound!=TIG_SOUND_HANDLE_INVALID)tig_sound_set_volume(sound,tig_sound_get_volume(sound)/3);
    }
}

static void tactical_camera_tick(void)
{
    if(!tactical_camera.moving || mainmenu_ui_is_active())return;
    if(!obj_handle_is_valid(tactical_camera.target)){
        tactical_camera.moving=false;tactical_camera.unlock=0;return;
    }
    Uint64 elapsed=SDL_GetTicks()-tactical_camera.started;
    double t=elapsed>=250?1.0:elapsed/250.0;
    double eased=t*t*(3.0-2.0*t);
    int64_t x,y;location_origin_get(&x,&y);
    location_origin_scroll(tactical_camera.x+(int64_t)llround(tactical_camera.dx*eased)-x,
        tactical_camera.y+(int64_t)llround(tactical_camera.dy*eased)-y);
    iso_invalidate_rect(NULL);
    if(elapsed>=250){
        location_origin_set(obj_field_int64_get(tactical_camera.target,OBJ_F_LOCATION));
        tactical_camera.moving=false;
        tactical_camera.unlock=SDL_GetTicks()+500;
    }
}

static bool tactical_camera_event(SDL_Event* event)
{
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP){
        int key=event->key.scancode;
        if(key>SDL_SCANCODE_UNKNOWN && key<SDL_SCANCODE_COUNT && tactical_camera.keys[key]){
            if(event->type==SDL_EVENT_KEY_UP)tactical_camera.keys[key]=false;
            return true;
        }
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_UP && (tactical_camera.buttons&SDL_BUTTON_MASK(event->button.button))){
        tactical_camera.buttons&=~SDL_BUTTON_MASK(event->button.button);
        return true;
    }
    if(!modern_combat_input_blocked())return false;
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN){
        tactical_camera.buttons|=SDL_BUTTON_MASK(event->button.button);
        return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP){
        int key=event->key.scancode;
        if(key==SDL_SCANCODE_ESCAPE)return false;
        if(event->type==SDL_EVENT_KEY_DOWN && key>SDL_SCANCODE_UNKNOWN && key<SDL_SCANCODE_COUNT)tactical_camera.keys[key]=true;
        return true;
    }
    return event->type==SDL_EVENT_MOUSE_BUTTON_UP || event->type==SDL_EVENT_MOUSE_WHEEL;
}

static bool tactical_region(float x,float y)
{
    return maintained_region(x,y) || party_region(x,y) || inside((SDL_FRect){(canvas_w-930)/2,10,930,66},x,y)
        || inside((SDL_FRect){(canvas_w-980)/2,canvas_h-126,980,116},x,y)
        || (tactical_log_open && inside((SDL_FRect){(canvas_w-980)/2,canvas_h-290,540,158},x,y));
}
static void tactical_tick(void)
{
    bool active=tactical_active();
    if(!active){
        tactical_camera.moving=false;tactical_camera.unlock=0;
        if(tactical_was_active){tactical_portraits_clear();tactical_actor=0;tactical_has_preview=false;}
        tactical_count=0;tactical_pressed_actor=0;
        memset(tactical_order,0,sizeof(tactical_order));
        tactical_was_active=false;return;
    }
    enabled=true;
    modern_combat_sync();
    int64_t actor=combat_turn_based_whos_turn_get();
    if(!tactical_was_active){tactical_line_count=0;tactical_page=0;tactical_log_open=false;}
    tactical_count=combat_turn_order(tactical_order,128);
    if(actor!=tactical_actor){
        tactical_camera.moving=false;tactical_camera.unlock=0;
        tactical_actor=actor;tactical_preview_time=0;tactical_has_preview=false;
        selector=0;tactical_spell_page=0;
        if(!mainmenu_ui_is_active() && inven_ui_is_created())inven_ui_destroy();
        pending_action=-1;pressed_action=-1;button_count=0;
        if(!mainmenu_ui_is_active() && (intgame_mode_get()==INTGAME_MODE_SPELL || intgame_mode_get()==INTGAME_MODE_SKILL || intgame_mode_get()==INTGAME_MODE_ITEM))intgame_mode_set(INTGAME_MODE_MAIN);
        for(int i=0;i<tactical_count;i++)if(tactical_order[i]==actor)tactical_page=i/10;
        if(actor && combat_player_controls(actor) && !mainmenu_ui_is_active())tactical_camera_focus(actor,true);
    }
    tactical_was_active=true;
    tactical_camera_tick();
    if(SDL_GetTicks()-tactical_preview_time>=80){
        tactical_preview_time=SDL_GetTicks();
        tactical_has_preview=!modern_combat_input_blocked() && !native_panel() && !native_menu() && !tactical_region(mouse_x,mouse_y)
            && modern_combat_preview(&tactical_preview);
    }
}
static SDL_FPoint tactical_point(int64_t loc)
{
    int64_t x,y;location_xy(loc,&x,&y);
    return (SDL_FPoint){(x+40)*canvas_w/hrp_iso_window_width_get(),(y+20)*canvas_h/hrp_iso_window_height_get()};
}
static void draw_tactical_preview(void)
{
    if(!tactical_has_preview || native_panel() || tactical_region(mouse_x,mouse_y))return;
    ModernCombatPreview* p=&tactical_preview;
    if(!p->actor || !obj_handle_is_valid(p->actor) || p->actor!=combat_input_actor())return;
    SDL_Color green={129,212,153,220},red={236,128,95,220};
    for(int i=1;i<p->count;i++){
        SDL_FPoint a=tactical_point(p->path[i-1]),b=tactical_point(p->path[i]);
        SDL_Color c=i*p->step_cost<=combat_action_points_get()?green:red;
        SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);
        SDL_RenderLine(renderer,a.x,a.y,b.x,b.y);ellipse(b.x,b.y,3,2,c);
    }
    if(p->radius>0 && p->radius<100){
        SDL_FPoint last={0};
        /* Arcanum's range metric is tile distance, not a D&D circle. */
        int64_t cx=location_get_x(p->location),cy=location_get_y(p->location);
        int radius=p->radius;
        for(int i=0;i<=4;i++){
            int dx[]={-1,1,1,-1,-1},dy[]={-1,-1,1,1,-1};
            SDL_FPoint pt=tactical_point(location_make(cx+dx[i]*radius,cy+dy[i]*radius));
            SDL_SetRenderDrawColor(renderer,210,178,105,190);
            if(i)SDL_RenderLine(renderer,last.x,last.y,pt.x,pt.y);last=pt;
        }
    }
    char line[512];
    const char* action=p->kind==0?tactical_text(TC_MOVE):p->kind==1?tactical_text(TC_ATTACK):game_text(spell_name(modern_spell_selected()));
    /* Movement is a quote: the original animation stack can charge a partial
     * first/last tile differently from the geometric path. */
    snprintf(line,sizeof(line),"%s  ·  %s %s%d %s",action,game_text(modern_intgame_text(15)),p->count>1?"~ ":"",p->cost,tactical_text(TC_AP));
    int remaining=combat_action_points_get()-p->cost;
    char budget[120];snprintf(budget,sizeof(budget),"  ·  %s %s%d %s",tr("Restants","Remaining"),p->count>1?"~":"",remaining>0?remaining:0,tactical_text(TC_AP));
    SDL_strlcat(line,budget,sizeof(line));
    char detail[512]="";
    if(!p->in_range)SDL_strlcat(detail,tactical_text(TC_RANGE),sizeof(detail));
    else if(!p->reachable)SDL_strlcat(detail,tactical_text(TC_BLOCKED),sizeof(detail));
    if(p->blocked)SDL_strlcat(detail,tr("  ·  Ligne de vue bloquée","  ·  Line of sight blocked"),sizeof(detail));
    if(!p->resources)SDL_strlcat(detail,tr("  ·  Fatigue insuffisante","  ·  Insufficient fatigue"),sizeof(detail));
    else if(p->reachable && !p->affordable)SDL_strlcat(detail,tr("  ·  PA insuffisants","  ·  Insufficient AP"),sizeof(detail));
    if(p->range>0){snprintf(budget,sizeof(budget),"  ·  %s %d",tr("Portée","Range"),p->range);SDL_strlcat(detail,budget,sizeof(detail));}
    if(p->chance>=0){snprintf(budget,sizeof(budget),"  ·  %s %d%%",game_text(modern_intgame_text(20)),p->chance);SDL_strlcat(detail,budget,sizeof(detail));}
    if(p->kind==1 && p->count>1)SDL_strlcat(detail,"  ·  ",sizeof(detail)),SDL_strlcat(detail,tactical_text(TC_AFTER_MOVE),sizeof(detail));
    if(p->fatigue)SDL_strlcat(detail,"  ·  ",sizeof(detail)),SDL_strlcat(detail,tactical_text(TC_FATIGUE),sizeof(detail));
    char health[1200]="";
    /* Exact party health is useful while aiming, including a one-point deficit.
     * Read the current engine values; do not predict healing or reveal enemies. */
    if(p->kind==2 && p->target && obj_handle_is_valid(p->target)
        && obj_type_is_critter(obj_field_int32_get(p->target,OBJ_F_TYPE))
        && (player_is_local_pc_obj(p->target) || player_is_local_pc_obj(critter_pc_leader_get(p->target)))) {
        char name[1024];object_examine(p->target,player_get_local_pc_obj(),name);
        int current=object_hp_current(p->target),maximum=object_hp_max(p->target);
        snprintf(health,sizeof(health),"%s  ·  %s : %d / %d",game_text(name),character_text(106),current,maximum);
        if(current<maximum){char deficit[40];snprintf(deficit,sizeof(deficit),"  (−%d)",maximum-current);SDL_strlcat(health,deficit,sizeof(health));}
    }
    float height=31+(*health?24:0)+(*detail?24:0);
    float width=fminf(900,fmaxf(text_measure(0,detail),fmaxf(text_measure(0,line),text_measure(0,health)))+42);
    float x=fminf(canvas_w-width-12,fmaxf(12,mouse_x-width/2)),y=fminf(canvas_h-137-height,mouse_y+36);
    fill((SDL_FRect){x,y,width,height},(SDL_Color){16,12,9,225});outline((SDL_FRect){x,y,width,height},p->affordable?green:red);
    text_compact(line,x+10,y+6,width-20,false,p->affordable?GOLD:red);
    if(*detail)text_compact(detail,x+10,y+30,width-20,false,p->affordable?MUTED:red);
    if(*health)text_compact(health,x+10,y+30+(*detail?24:0),width-20,false,GOLD);
}
static void tactical_button(SDL_FRect r,int action,const char* label,bool selected,bool available)
{
    button(r,action,"",-1,selected,available);
    if(!control_symbol(r,label,available,selected || (available && inside(r,mouse_x,mouse_y))))
        text_compact(label,r.x+5,r.y+(r.h-fonts[0].size)/2,r.w-10,true,available?GOLD:MUTED);
}
static void draw_tactical_hud(void)
{
    float tx=(canvas_w-930)/2;frame((SDL_FRect){tx,10,930,66});
    char round[40];snprintf(round,sizeof(round),"%s %d",tactical_text(TC_ROUND),combat_turn_based_turn_get());
    text_draw(0,round,tx+13,18,GOLD);
    button((SDL_FRect){tx+12,40,29,25},1101,"<",-1,false,tactical_page>0);
    button((SDL_FRect){tx+47,40,29,25},1102,">",-1,false,(tactical_page+1)*10<tactical_count);
    for(int j=0;j<10;j++){
        int i=tactical_page*10+j;if(i>=tactical_count)break;
        int64_t obj=tactical_order_actor(i);if(!obj)continue;
        float x=tx+90+j*80;
        SDL_FRect rect={x,16,74,53};bool selected=obj==tactical_actor;
        button(rect,1200+i,"",-1,selected,true);
        if(combat_shared_turn_available(obj))fill((SDL_FRect){x+2,70,70,3},GOLD);
        tactical_portrait(obj,(SDL_FRect){x+4,19,31,40});
        char name[1024];object_examine(obj,player_get_local_pc_obj(),name);
        text_compact(game_text(name),x+38,20,32,false,selected?GOLD:MUTED);
        int hp=object_hp_max(obj);float fraction=hp>0?(float)object_hp_current(obj)/hp:0;
        fill((SDL_FRect){x+38,49,30*fmaxf(0,fminf(1,fraction)),4},combat_player_controls(obj)?(SDL_Color){107,176,116,255}:(SDL_Color){190,74,57,255});
        if(inside(rect,mouse_x,mouse_y)){
            const char* n=game_text(name);float w=text_measure(1,n)+20;
            fill((SDL_FRect){x,80,w,30},(SDL_Color){16,12,9,235});text_draw(1,n,x+10,82,GOLD);
        }
    }
    float x=(canvas_w-980)/2,y=canvas_h-126;frame((SDL_FRect){x,y,980,116});
    int64_t actor=tactical_actor;if(!actor || !obj_handle_is_valid(actor))return;
    bool ready=combat_input_ready(actor),owned=combat_player_controls(actor);
    gauge(x+27,y+9,object_hp_current(actor),object_hp_max(actor),false);
    gauge(x+915,y+9,critter_fatigue_current(actor),critter_fatigue_max(actor),true);
    tactical_portrait(actor,(SDL_FRect){x+100,y+12,50,65});
    char name[1024];object_examine(actor,player_get_local_pc_obj(),name);
    text_compact(game_text(name),x+165,y+10,375,false,GOLD);
    char points[120];int max=stat_level_get(actor,STAT_SPEED);if(max<5)max=5;
    snprintf(points,sizeof(points),"%s  %d / %d",tactical_text(TC_AP),combat_action_points_get(),max);
    text_draw(1,points,x+165,y+32,GOLD);
    bool preview=tactical_has_preview && tactical_preview.actor==actor && !native_panel() && !tactical_region(mouse_x,mouse_y);
    int after=preview?combat_action_points_get()-tactical_preview.cost:combat_action_points_get();
    if(preview){char next[96];snprintf(next,sizeof(next),"%s %s%d %s",tr("Après :","After:"),tactical_preview.count>1?"~":"",after>0?after:0,tactical_text(TC_AP));text_compact(next,x+350,y+35,185,false,tactical_preview.affordable?GOLD:(SDL_Color){236,128,95,255});}
    if(!ready)text_compact(tactical_text(owned?TC_BUSY:TC_WAIT),x+350,y+35,185,false,MUTED);
    int pa=combat_action_points_get();
    int cells=max>30?30:max;
    for(int i=0;i<cells;i++)fill((SDL_FRect){x+165+i*370.0f/cells,y+62,370.0f/cells-3,8},i*max<pa*cells?(preview && i*max>=after*cells?(SDL_Color){215,157,70,255}:(SDL_Color){126,173,108,255}):(SDL_Color){57,49,35,255});
    const int ids[]={1103,6,5,1104};
    const char* names[]={tactical_text(TC_ATTACK),game_text(modern_intgame_text(1000)),game_text(modern_intgame_text(1001)),game_text(modern_intgame_text(1006))};
    for(int i=0;i<4;i++)tactical_button((SDL_FRect){x+100+i*112,y+82,106,25},ids[i],names[i],false,ready);
    if(player_is_local_pc_obj(actor))for(int i=0;i<10;i++){
        SDL_FRect r={x+550+(i%5)*43,y+13+(i/5)*38,38,34};button(r,300+i,"",-1,false,ready);
        Hotkey* hk=sub_57F240(i);if(hk && hk->type!=HOTKEY_NONE)draw_art_id(hk->art_id,(SDL_FRect){r.x+4,r.y+2,29,26},true);
        char digit[3];snprintf(digit,sizeof(digit),"%d",(i+1)%10);text_draw(0,digit,r.x+1,r.y+19,MUTED);
    }
    else {
        button((SDL_FRect){x+550,y+15,210,28},951,game_text(sub_57A700(0)),sub_579F50(0),false,ready);
        tactical_button((SDL_FRect){x+550,y+50,210,28},1105,game_text(modern_intgame_text(1005)),false,ready);
    }
    tactical_button((SDL_FRect){x+550,y+83,210,24},1106,tactical_text(TC_LOG),tactical_log_open,true);
    tactical_button((SDL_FRect){x+775,y+18,125,56},1100,tactical_text(TC_END),false,ready);
    centered(0,key_name(KB_END),x+837,y+84,MUTED);
    if(*message_text && SDL_GetTicks()-message_time<8000){
        float width=fminf(940,text_measure(0,message_text)+32);
        SDL_FRect r={(canvas_w-width)/2,y-39,width,30};
        fill(r,(SDL_Color){16,12,9,235});outline(r,GOLD);
        text_compact(message_text,r.x+12,r.y+6,r.w-24,true,GOLD);
    }
    if(tactical_log_open){
        SDL_FRect r={x,y-164,540,158};fill(r,(SDL_Color){16,12,9,220});outline(r,GOLD);
        int first=tactical_line_count>6?tactical_line_count-6:0;
        for(int i=first;i<tactical_line_count;i++)text_compact(tactical_lines[i%32],x+10,r.y+9+(i-first)*24,520,false,MUTED);
    }
}
static bool tactical_action(int action)
{
    if(!tactical_active())return false;
    if(action==1101){if(tactical_page>0)tactical_page--;return true;}
    if(action==1102){if((tactical_page+1)*10<tactical_count)tactical_page++;return true;}
    if(action==1110){if(tactical_spell_page>0)tactical_spell_page--;return true;}
    if(action==1111){if(tactical_spell_page<7)tactical_spell_page++;return true;}
    if(action==1106){tactical_log_open=!tactical_log_open;return true;}
    if(action>=1200 && action<1328){
        int64_t obj=tactical_order_actor(action-1200);
        if(obj && !combat_shared_turn_select(obj))tactical_camera_focus(obj,false);
        return true;
    }
    bool command=action==5 || action==6 || action==950 || action==951
        || (action>=300 && action<310) || (action>=870 && action<884)
        || (action>=1100 && action<=1105) || (action>=1400 && action<1480) || (action>=1500 && action<1532);
    if(!command)return false;
    int64_t actor=combat_input_actor();
    if(!combat_input_ready(actor) || actor!=tactical_actor)return true;
    if(action>=1400 && action<1480){
        int spell=action-1400,cost;
        bool ap=combat_action_points_get()>=4 || (combat_action_points_get()>0 && player_is_local_pc_obj(actor) && critter_fatigue_current(actor)>1);
        if(ap && modern_spell_resources(actor,spell,&cost)) {selector=0;spell_ui_activate(actor,spell);}return true;
    }
    if(action>=1500 && action<1532){int i=action-1500;if(i<tactical_maintained_count)modern_spell_stop(actor,tactical_maintained_ids[i]);return true;}
    if(action==1100){if(actor==tactical_pressed_actor)combat_input_end_turn(actor);return true;}
    if(action==1103){intgame_mode_set(INTGAME_MODE_MAIN);combat_critter_activate_combat_mode(actor);return true;}
    if(action==1104){inven_ui_open(actor,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY);return true;}
    if(action==1105){charedit_open(actor,CHAREDIT_MODE_PASSIVE);return true;}
    return false;
}


static void draw_tactical_spells(void)
{
    int64_t actor=combat_input_actor();int known[80],count=0;
    for(int spell=0;spell<SPELL_COUNT;spell++)if(spell_is_known(actor,spell))known[count++]=spell;
    int pages=(count+9)/10;if(pages<1)pages=1;
    if(tactical_spell_page>=pages)tactical_spell_page=pages-1;
    int visible=count-tactical_spell_page*10;if(visible>10)visible=10;
    int rows=(visible+1)/2;
    tactical_maintained_count=modern_maintained_spells(actor,tactical_maintained_ids,tactical_maintained_spells,32);
    float width=850,height=185+fmaxf(1,rows)*78+(tactical_maintained_count?85:0);
    float x=(canvas_w-width)/2,y=(canvas_h-height)/2;
    frame((SDL_FRect){x,y,width,height});
    centered(2,game_text(modern_intgame_text(1000)),canvas_w/2,y+20,GOLD);
    text_compact(actor_name(actor),x+25,y+55,width-50,true,MUTED);
    bool ready=combat_input_ready(actor);
    bool ap=combat_action_points_get()>=4 || (combat_action_points_get()>0 && player_is_local_pc_obj(actor) && critter_fatigue_current(actor)>1);
    for(int i=0;i<visible;i++){
        int spell=known[tactical_spell_page*10+i],cost;
        bool resources=modern_spell_resources(actor,spell,&cost);
        bool available=ready && ap && resources;
        SDL_FRect r={x+25+(i%2)*400,y+91+(i/2)*78,390,69};
        button(r,1400+spell,"",-1,false,available);
        draw_art(spell_icon(spell),(SDL_FRect){r.x+8,r.y+10,44,44},true);
        text_compact(game_text(spell_name(spell)),r.x+62,r.y+10,r.w-72,false,available?GOLD:MUTED);
        char price[100];snprintf(price,sizeof(price),"4 %s  ·  %s %d",tactical_text(TC_AP),game_text(modern_intgame_text(44)),cost);
        if(!available)snprintf(price,sizeof(price),"%s",!ready?tactical_text(TC_BUSY):!ap?tr("PA insuffisants","Insufficient AP"):tr("Fatigue insuffisante","Insufficient fatigue"));
        text_compact(price,r.x+62,r.y+39,r.w-72,false,available?GOLD:(SDL_Color){210,134,103,255});
    }
    if(!count)centered(1,"—",canvas_w/2,y+111,MUTED);
    if(tactical_maintained_count){
        float yy=y+94+fmaxf(1,rows)*78;
        text_draw(0,tactical_text(TC_MAINTAINED),x+25,yy,MUTED);
        for(int i=0;i<tactical_maintained_count && i<10;i++){
            int spell=tactical_maintained_spells[i];SDL_FRect r={x+25+i*78,yy+25,71,45};
            button(r,1500+i,"×",spell_icon(spell),false,ready);
            if(inside(r,mouse_x,mouse_y)){
                const char* name=game_text(spell_name(spell));float w=text_measure(0,name)+24;
                fill((SDL_FRect){r.x,yy-32,w,28},(SDL_Color){16,12,9,245});text_draw(0,name,r.x+12,yy-27,GOLD);
            }
        }
    }
    tactical_button((SDL_FRect){x+25,y+height-57,45,34},1110,"<",false,tactical_spell_page>0);
    tactical_button((SDL_FRect){x+140,y+height-57,45,34},1111,">",false,tactical_spell_page+1<pages);
    char page[32];snprintf(page,sizeof(page),"%d / %d",tactical_spell_page+1,pages);centered(0,page,x+105,y+height-49,MUTED);
    tactical_button((SDL_FRect){x+width-230,y+height-57,205,34},840,menu_text(54),false,true);
}
