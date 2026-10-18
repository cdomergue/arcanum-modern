/* Tactical presentation. Order, PA, rolls and execution belong to the engine. */
static int64_t tactical_actor, tactical_pressed_actor;
static int64_t tactical_order[128];
static int tactical_count, tactical_page;
static bool tactical_was_active, tactical_log_open;
static ModernCombatPreview tactical_preview;
static bool tactical_has_preview;
static Uint64 tactical_preview_time;
static char tactical_lines[32][1200];
static int tactical_line_count;
static struct { int number; SDL_Texture* texture; float width,height; } tactical_portraits[32];
static int tactical_portrait_count;

/* Only newly introduced controls need new strings. All game names, statistics,
 * spells, costs and outcomes below come from the installed localization. */
enum {TC_END,TC_AP,TC_ROUND,TC_WAIT,TC_MOVE,TC_ATTACK,TC_BLOCKED,TC_RANGE,TC_FATIGUE,TC_BUSY,TC_LOG,TC_AFTER_MOVE};
static const char* tactical_text(int id)
{
    static const char* en[]={"End turn","AP","Round","Enemy turn","Move","Attack","No path / blocked","Out of range","Fatigue","Action in progress","Combat log","After approach"};
    static const char* fr[]={"Fin du tour","PA","Tour","Tour adverse","Déplacement","Attaque","Chemin bloqué","Hors de portée","Fatigue","Action en cours","Journal de combat","Après approche"};
    return strcmp(modern_intgame_text(0),"Niveau")==0?fr[id]:en[id];
}

void modern_combat_log(int64_t object,const char* raw)
{
    if(!enabled || !combat_turn_based_is_active() || !raw || !*raw || !obj_handle_is_valid(object))return;
    char name[1000],text[1000];object_examine(object,player_get_local_pc_obj(),name);
    from_game(raw,text,sizeof(text));
    char line[1200];snprintf(line,sizeof(line),"%s : %.900s",game_text(name),text);
    if(tactical_line_count && strcmp(tactical_lines[(tactical_line_count-1)%32],line)==0)return;
    SDL_strlcpy(tactical_lines[tactical_line_count++%32],line,sizeof(tactical_lines[0]));
}

static void tactical_portraits_clear(void)
{
    for(int i=0;i<tactical_portrait_count;i++)SDL_DestroyTexture(tactical_portraits[i].texture);
    tactical_portrait_count=0;
}
static void tactical_portrait(int64_t actor,SDL_FRect rect)
{
    int number=-1;bool portrait=intgame_examine_portrait(player_get_local_pc_obj(),actor,&number);
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
static bool tactical_region(float x,float y)
{
    return inside((SDL_FRect){(canvas_w-930)/2,10,930,66},x,y)
        || inside((SDL_FRect){(canvas_w-980)/2,canvas_h-126,980,116},x,y)
        || (tactical_log_open && inside((SDL_FRect){(canvas_w-980)/2,canvas_h-290,540,158},x,y));
}
static void tactical_tick(void)
{
    bool active=tactical_active();
    if(!active){
        if(tactical_was_active){tactical_portraits_clear();tactical_actor=0;tactical_has_preview=false;}
        tactical_was_active=false;return;
    }
    enabled=true;
    modern_combat_sync();
    int64_t actor=combat_turn_based_whos_turn_get();
    if(!tactical_was_active){tactical_line_count=0;tactical_page=0;tactical_log_open=false;}
    tactical_count=combat_turn_order(tactical_order,128);
    if(actor!=tactical_actor){
        tactical_actor=actor;tactical_preview_time=0;tactical_has_preview=false;
        selector=0;
        if(!mainmenu_ui_is_active() && inven_ui_is_created())inven_ui_destroy();
        pending_action=-1;pressed_action=-1;button_count=0;
        if(!mainmenu_ui_is_active() && (intgame_mode_get()==INTGAME_MODE_SPELL || intgame_mode_get()==INTGAME_MODE_SKILL || intgame_mode_get()==INTGAME_MODE_ITEM))intgame_mode_set(INTGAME_MODE_MAIN);
        for(int i=0;i<tactical_count;i++)if(tactical_order[i]==actor)tactical_page=i/10;
        if(actor && combat_player_controls(actor) && !mainmenu_ui_is_active())location_origin_set(obj_field_int64_get(actor,OBJ_F_LOCATION));
    }
    tactical_was_active=true;
    if(SDL_GetTicks()-tactical_preview_time>=80){
        tactical_preview_time=SDL_GetTicks();
        tactical_has_preview=!native_panel() && !native_menu() && !tactical_region(mouse_x,mouse_y)
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
    if(p->actor!=combat_input_actor())return;
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
    snprintf(line,sizeof(line),"%s  ·  %s %d %s",action,game_text(modern_intgame_text(15)),p->cost,tactical_text(TC_AP));
    if(!p->in_range)SDL_strlcat(line,"  ·  ",sizeof(line)),SDL_strlcat(line,tactical_text(TC_RANGE),sizeof(line));
    else if(!p->reachable)SDL_strlcat(line,"  ·  ",sizeof(line)),SDL_strlcat(line,tactical_text(TC_BLOCKED),sizeof(line));
    if(p->chance>=0){char chance[64];snprintf(chance,sizeof(chance),"  ·  %s %d%%",game_text(modern_intgame_text(20)),p->chance);SDL_strlcat(line,chance,sizeof(line));}
    if(p->kind==1 && p->count>1)SDL_strlcat(line,"  ·  ",sizeof(line)),SDL_strlcat(line,tactical_text(TC_AFTER_MOVE),sizeof(line));
    if(p->fatigue)SDL_strlcat(line,"  ·  ",sizeof(line)),SDL_strlcat(line,tactical_text(TC_FATIGUE),sizeof(line));
    float width=fminf(900,text_measure(0,line)+42),x=fminf(canvas_w-width-12,fmaxf(12,mouse_x-width/2)),y=fminf(canvas_h-168,mouse_y+36);
    fill((SDL_FRect){x,y,width,31},(SDL_Color){16,12,9,225});outline((SDL_FRect){x,y,width,31},p->affordable?green:red);
    text_compact(line,x+10,y+6,width-20,false,p->affordable?GOLD:red);
}
static void tactical_button(SDL_FRect r,int action,const char* label,bool selected,bool available)
{
    button(r,action,"",-1,selected,available);
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
        int64_t obj=tactical_order[i];float x=tx+90+j*80;
        SDL_FRect rect={x,16,74,53};bool selected=obj==tactical_actor;
        button(rect,1200+i,"",-1,selected,true);
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
    if(!ready)text_compact(tactical_text(owned?TC_BUSY:TC_WAIT),x+350,y+35,185,false,MUTED);
    int pa=combat_action_points_get();
    int cells=max>30?30:max;
    for(int i=0;i<cells;i++)fill((SDL_FRect){x+165+i*370.0f/cells,y+62,370.0f/cells-3,8},i*max<pa*cells?(SDL_Color){126,173,108,255}:(SDL_Color){57,49,35,255});
    const int ids[]={1103,6,5,1104};
    const char* names[]={tactical_text(TC_ATTACK),game_text(modern_intgame_text(1000)),game_text(modern_intgame_text(1001)),game_text(modern_intgame_text(1006))};
    for(int i=0;i<4;i++)tactical_button((SDL_FRect){x+100+i*112,y+82,106,25},ids[i],names[i],false,ready);
    if(player_is_local_pc_obj(actor))for(int i=0;i<10;i++){
        SDL_FRect r={x+550+(i%5)*43,y+13+(i/5)*38,38,34};button(r,300+i,"",-1,false,ready);
        Hotkey* hk=sub_57F240(i);if(hk && hk->type!=HOTKEY_NONE)draw_art_id(hk->art_id,(SDL_FRect){r.x+4,r.y+2,29,26},true);
        char digit[3];snprintf(digit,sizeof(digit),"%d",(i+1)%10);text_draw(0,digit,r.x+1,r.y+19,MUTED);
    }
    else {
        tactical_button((SDL_FRect){x+550,y+15,210,28},951,game_text(sub_57A700(0)),false,ready);
        tactical_button((SDL_FRect){x+550,y+50,210,28},1105,game_text(modern_intgame_text(1005)),false,ready);
    }
    tactical_button((SDL_FRect){x+550,y+83,210,24},1106,tactical_text(TC_LOG),tactical_log_open,true);
    tactical_button((SDL_FRect){x+775,y+18,125,56},1100,tactical_text(TC_END),false,ready);
    centered(0,"[E / Space]",x+837,y+84,MUTED);
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
    if(action==1106){tactical_log_open=!tactical_log_open;return true;}
    if(action>=1200 && action<1328){
        int i=action-1200;if(i<tactical_count && obj_handle_is_valid(tactical_order[i]))location_origin_set(obj_field_int64_get(tactical_order[i],OBJ_F_LOCATION));
        return true; /* Camera only: selecting a portrait never changes turn/AP. */
    }
    bool command=action==5 || action==6 || action==950 || action==951
        || (action>=300 && action<310) || (action>=870 && action<884)
        || (action>=1100 && action<=1105);
    if(!command)return false;
    int64_t actor=combat_input_actor();
    if(!combat_input_ready(actor) || actor!=tactical_actor)return true;
    if(action==1100){if(actor==tactical_pressed_actor)combat_input_end_turn(actor);return true;}
    if(action==1103){intgame_mode_set(INTGAME_MODE_MAIN);combat_critter_activate_combat_mode(actor);return true;}
    if(action==1104){inven_ui_open(actor,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY);return true;}
    if(action==1105){charedit_open(actor,CHAREDIT_MODE_PASSIVE);return true;}
    return false;
}
