/* Native play screens. Included after the shared rendering helpers. */
typedef struct PanelRegion { SDL_FRect rect; int x,y,w,h,kind; } PanelRegion;
static PanelRegion panel_regions[32];
static int panel_region_count, selector, selector_college, panel_previous;
static int dialogue_scroll, dialogue_max;
static int written_scroll,written_max,written_step=3;
static char dialogue_previous[1000];
static SDL_FRect dialogue_view;
static int dialogue_choice=-1;
static struct { int kind,x,y,event; } panel_events[64];
static int panel_event_count;
static SDL_Texture* map_texture;
static SDL_Texture* inventory_actions_texture;
static Uint64 map_texture_time;
static int map_texture_mode=-1;
static int64_t inventory_hover,inventory_detail_object;
static int inventory_detail_kind,inventory_detail_scroll,inventory_detail_max;
static ModernDetails inventory_details;
static SDL_FRect inventory_detail_rect;
static char inventory_query[128];
static int inventory_filter,inventory_compare_scroll,inventory_compare_max;
static bool inventory_searching,inventory_compare=true;
static SDL_FRect inventory_compare_rect;
static bool inventory_quick_release;
static int inventory_quick_x,inventory_quick_y;
static SDL_FRect inventory_quick_region;
static int64_t inventory_quick_item;
static void inventory_search_focus(bool on)
{
    inventory_searching=on;if(on)SDL_StartTextInput(window);else SDL_StopTextInput(window);
}
static bool inventory_match(int64_t obj)
{
    if(!obj || !obj_handle_is_valid(obj))return false;
    int type=obj_field_int32_get(obj,OBJ_F_TYPE);
    int category=type==OBJ_TYPE_WEAPON?1:type==OBJ_TYPE_ARMOR?2:
        (type==OBJ_TYPE_FOOD || type==OBJ_TYPE_SCROLL)?3:4;
    if(inventory_filter && inventory_filter!=category)return false;
    if(*inventory_query){
        char raw[2048],name[4096],needle[256];object_examine(obj,player_get_local_pc_obj(),raw);
        fold(game_text(raw),name,sizeof(name));fold(inventory_query,needle,sizeof(needle));
        if(!strstr(name,needle))return false;
    }
    return true;
}
static void inventory_query_edit(const char* text,bool backspace)
{
    if(backspace){size_t n=strlen(inventory_query);if(n){do{n--;}while(n && ((unsigned char)inventory_query[n]&0xc0)==0x80);inventory_query[n]=0;}}
    else if(strlen(inventory_query)+strlen(text)<sizeof(inventory_query))SDL_strlcat(inventory_query,text,sizeof(inventory_query));
}
static bool map_pressed,map_dragged;
static PanelRegion map_drag_region;
static float map_start_x,map_start_y,map_last_x,map_last_y,map_pan_x,map_pan_y;
static int map_press_x,map_press_y;
#include "modern_map_camera.inc"
static MapCamera map_camera;
static ModernLocalMap map_local;
static SDL_FRect map_viewport,map_destination;
static bool map_camera_active;
static int map_camera_id=-1;

static void map_zoom(float factor,float x,float y)
{
    if(!map_camera_active || map_pressed)return;
    map_camera_zoom(&map_camera,factor,x-map_destination.x,y-map_destination.y,
        map_viewport.w,map_viewport.h,map_local.width,map_local.height);
}


static bool play_sheet(void) { return !mainmenu_ui_is_active() && charedit_is_created(); }
static int native_panel(void)
{
    if(!enabled || !in_game || menu_suspended)return 0;
    if(mainmenu_ui_is_active())return mainmenu_ui_window_type_get()==MM_WINDOW_SHOP && inven_ui_is_created()?2:0;
    if(modern_written_text())return 9;
    if(play_sheet())return 1;
    if(inven_ui_is_created())return 2;
    if(wmap_ui_is_created())return 3;
    if(modern_dialog_state())return 4;
    if(modern_sleep_count())return 6;
    ModernSchematic scheme;if(modern_schematic_state(&scheme))return 7;
    if(modern_fate_count())return 8;
    if(selector)return 5;
    return 0;
}
bool modern_ui_open_selector(bool spells)
{
    if(!enabled || !in_game || !fonts[0].texture || mainmenu_ui_is_active())return false;
    int p=native_panel();
    if(!p || p==5)selector=spells?1:2;
    return true;
}
static void panel_region(SDL_FRect rect,int x,int y,int w,int h,int kind)
{
    if(panel_region_count<(int)SDL_arraysize(panel_regions))
        panel_regions[panel_region_count++]=(PanelRegion){rect,x,y,w,h,kind};
}
static void panel_mouse_queue(int kind,int x,int y,int event)
{
    if(panel_event_count>0 && event==TIG_MESSAGE_MOUSE_MOVE
        && panel_events[panel_event_count-1].event==event)panel_event_count--;
    if(panel_event_count<(int)SDL_arraysize(panel_events)) {
        panel_events[panel_event_count].kind=kind;panel_events[panel_event_count].x=x;
        panel_events[panel_event_count].y=y;panel_events[panel_event_count++].event=event;
    }
}
typedef struct QuickBinding { int type,data,slot; int64_t item,actor; } QuickBinding;
static QuickBinding binding_drag,binding_pending;
static bool binding_pressed,binding_moved,binding_cancel;
static float binding_x,binding_y;
static SDL_FRect binding_slots[10];
static bool binding_bar_visible;

static QuickBinding binding_hover(void)
{
    QuickBinding result={.actor=combat_input_actor(),.slot=-1};
    int p=native_panel();
    if(p==2 && intgame_mode_get()!=INTGAME_MODE_QUANTITY) {
        for(int i=0;i<panel_region_count;i++) {
            PanelRegion* r=&panel_regions[i];
            if(r->kind!=2 || !inside(r->rect,mouse_x,mouse_y))continue;
            int x=r->x+(mouse_x-r->rect.x)*r->w/r->rect.w;
            int y=r->y+(mouse_y-r->rect.y)*r->h/r->rect.h;
            int64_t item=modern_inventory_at(x,y),owner;
            if(item && (!r->w || inventory_match(item)) && item_parent(item,&owner) && owner==result.actor) {
                int type=obj_field_int32_get(item,OBJ_F_TYPE);
                if(type!=OBJ_TYPE_GOLD && type!=OBJ_TYPE_AMMO) {result.type=HOTKEY_ITEM;result.item=item;}
            }
            return result;
        }
    }
    if(p==5)for(int i=button_count-1;i>=0;i--) {
        if(!inside(buttons[i].rect,mouse_x,mouse_y))continue;
        int action=buttons[i].action;
        if(action>=870 && action<875) {result.type=HOTKEY_SPELL;result.data=selector_college*5+action-870;}
        if(action>=1400 && action<1480) {result.type=HOTKEY_SPELL;result.data=action-1400;}
        if(action>=880 && action<884) {result.type=HOTKEY_SKILL;result.data=action-880;}
        if(result.type==HOTKEY_SPELL && !spell_is_known(result.actor,result.data))result.type=HOTKEY_NONE;
        return result;
    }
    return result;
}
static int binding_slot_at(float x,float y)
{
    if(binding_bar_visible)for(int i=0;i<10;i++)if(inside(binding_slots[i],x,y))return i;
    return -1;
}
static void binding_tick(void)
{
    if(binding_pending.type) {
        QuickBinding request=binding_pending;binding_pending.type=HOTKEY_NONE;
        if(request.actor==combat_input_actor())modern_hotkey_assign(request.slot,request.type,request.data,request.item);
        sub_575770();sub_573730();intgame_refresh_cursor();
    }
    if(binding_cancel){sub_575770();sub_573730();intgame_refresh_cursor();binding_cancel=false;}
}
static void draw_binding_bar(void)
{
    int p=native_panel();binding_bar_visible=(p==2 || p==5) && intgame_mode_get()!=INTGAME_MODE_QUANTITY;
    if(!binding_bar_visible)return;
    float x=(canvas_w-610)/2,y=canvas_h-54;
    fill((SDL_FRect){x-10,y-5,620,53},(SDL_Color){18,13,9,245});
    for(int i=0;i<10;i++) {
        SDL_FRect r={x+i*60,y,54,42};binding_slots[i]=r;
        fill(r,(SDL_Color){40,29,18,255});outline(r,inside(r,mouse_x,mouse_y)?GOLD:MUTED);
        Hotkey* hk=sub_57F240(i);
        if(hk && hk->type!=HOTKEY_NONE)draw_art_id(hk->art_id,(SDL_FRect){r.x+13,r.y+2,34,34},true);
        char number[4];snprintf(number,sizeof(number),"%d",(i+1)%10);text_draw(0,number,r.x+3,r.y+22,GOLD);
    }
    if(binding_pressed && binding_moved) {
        if(binding_drag.type==HOTKEY_ITEM && obj_handle_is_valid(binding_drag.item))
            draw_art_id(obj_field_int32_get(binding_drag.item,OBJ_F_ITEM_INV_AID),(SDL_FRect){mouse_x+12,mouse_y+12,48,48},true);
        else if(binding_drag.type==HOTKEY_SPELL)draw_art(spell_icon(binding_drag.data),(SDL_FRect){mouse_x+12,mouse_y+12,48,48},true);
        else if(binding_drag.type==HOTKEY_SKILL)draw_art(sub_579F50(binding_drag.data),(SDL_FRect){mouse_x+12,mouse_y+12,48,48},true);
    }
}
static bool binding_event(SDL_Event* event)
{
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
        binding_cancel=binding_pressed;binding_pressed=binding_moved=false;return false;
    }
    if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat && intgame_mode_get()!=INTGAME_MODE_QUANTITY) {
        int key=event->key.scancode,slot=-1;
        if(key>=SDL_SCANCODE_1 && key<=SDL_SCANCODE_0)slot=key-SDL_SCANCODE_1;
        if(slot>=0) {
            QuickBinding source=binding_hover();
            if(source.type){source.slot=slot;binding_pending=source;}
            return true;
        }
        if(event->key.scancode==SDL_SCANCODE_ESCAPE && binding_pressed) {
            binding_cancel=true;binding_pressed=binding_moved=false;pressed_action=-1;click_captured=false;return true;
        }
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button==SDL_BUTTON_LEFT) {
        binding_drag=binding_hover();binding_pressed=binding_drag.type!=HOTKEY_NONE;binding_moved=false;
        binding_x=mouse_x;binding_y=mouse_y;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION && binding_pressed) {
        float dx=mouse_x-binding_x,dy=mouse_y-binding_y;
        if(dx*dx+dy*dy>=36)binding_moved=true;
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_UP && event->button.button==SDL_BUTTON_LEFT && binding_pressed) {
        bool moved=binding_moved;binding_pressed=binding_moved=false;
        int slot=binding_slot_at(mouse_x,mouse_y);
        if(moved && slot>=0) {
            binding_drag.slot=slot;binding_pending=binding_drag;
            pressed_action=-1;click_captured=false;return true;
        }
        if(moved && binding_drag.type!=HOTKEY_ITEM) {pressed_action=-1;click_captured=false;return true;}
    }
    if(binding_slot_at(mouse_x,mouse_y)>=0 && (event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP))return true;
    return false;
}

/* A numbered assignment over an item ends text focus before SDL emits text. */
static bool inventory_search_event(SDL_Event* event)
{
    if(inventory_searching && event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat
        && event->key.scancode>=SDL_SCANCODE_1 && event->key.scancode<=SDL_SCANCODE_0
        && binding_hover().type!=HOTKEY_NONE) {
        inventory_search_focus(false);
        return binding_event(event);
    }
    if(inventory_searching && event->type==SDL_EVENT_MOUSE_BUTTON_DOWN
        && hit(mouse_x,mouse_y)!=2400)inventory_search_focus(false);
    if(inventory_searching){
        if(event->type==SDL_EVENT_TEXT_INPUT){inventory_query_edit(event->text.text,false);return true;}
        if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP){
            if(event->type==SDL_EVENT_KEY_DOWN){
                if(event->key.scancode==SDL_SCANCODE_ESCAPE || event->key.scancode==SDL_SCANCODE_RETURN)inventory_search_focus(false);
                else if(event->key.scancode==SDL_SCANCODE_BACKSPACE)inventory_query_edit("",true);
            }return true;
        }
    }
    return false;
}

static void panel_backdrop(void)
{
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){15,12,10,255});
    material(wood_texture,(SDL_FRect){0,0,canvas_w,canvas_h});
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){0,0,0,115});
}
static const char* actor_name(int64_t obj)
{
    static char text[1024];text[0]=0;
    if(obj)object_examine(obj,player_get_local_pc_obj(),text);
    return game_text(text);
}
static void inventory_grid(SDL_FRect r,int legacy_x,int legacy_y,int rows,int scroll)
{
    float cell=r.w/10;r.h=rows*cell;
    fill(r,(SDL_Color){21,18,14,255});outline(r,MUTED);
    for(int col=1;col<10;col++)fill((SDL_FRect){r.x+col*cell,r.y,1,r.h},(SDL_Color){68,52,32,255});
    for(int row=1;row<rows;row++)rule(r.x,r.y+row*cell,r.w,(SDL_Color){68,52,32,255});
    panel_region(r,legacy_x,legacy_y,320,rows*32,2);
    SDL_Rect clip={(int)r.x,(int)r.y,(int)r.w,(int)r.h};SDL_SetRenderClipRect(renderer,&clip);
    int64_t seen[120];int seen_count=0;
    for(int row=0;row<rows;row++)for(int col=0;col<10;col++) {
        int64_t obj=modern_inventory_at(legacy_x+col*32+16,legacy_y+row*32+16);
        if(!obj || obj==inven_ui_drag_item_obj_get() || !inventory_match(obj))continue;
        bool duplicate=false;for(int k=0;k<seen_count;k++)if(seen[k]==obj)duplicate=true;
        if(duplicate)continue;seen[seen_count++]=obj;
        int loc=item_inventory_location_get(obj),iw,ih;item_inv_icon_size(obj,&iw,&ih);
        int rr=loc/10-scroll,cc=loc%10;
        SDL_FRect icon={r.x+cc*cell+3,r.y+rr*cell+3,iw*cell-6,ih*cell-6};
        SDL_FRect footprint={r.x+cc*cell,r.y+rr*cell,iw*cell,ih*cell};
        if(inside(r,mouse_x,mouse_y) && inside(footprint,mouse_x,mouse_y)) {
            inventory_hover=obj;
            fill(footprint,(SDL_Color){104,61,21,215});outline(footprint,GOLD);
        }
        draw_art_id(obj_field_int32_get(obj,OBJ_F_ITEM_INV_AID),icon,true);
        int quantity_field;
        if(sub_462410(obj,&quantity_field)!=-1) {
            char quantity[32];snprintf(quantity,sizeof(quantity),"%d",obj_field_int32_get(obj,quantity_field));
            text_draw(0,quantity,icon.x+3,icon.y+icon.h-17,GOLD);
        }
    }
    SDL_SetRenderClipRect(renderer,NULL);
}
static void equipment_slots(SDL_FRect r,int side,int mode)
{
    /* Original hit targets, used only by the engine adapter. Our layout is a
       symmetrical equipment rack with separate, generous native hit boxes. */
    const int coords[9][4]={{151,107,64,64},{247,107,32,32},{279,107,32,32},{247,139,64,32},
        {23,170,96,128},{247,171,96,128},{119,171,128,160},{55,107,64,64},{150,331,64,64}};
    const int target[9][4]={{157,182,42,42},{220,182,21,21},{241,182,21,21},{220,203,42,21},
        {73,223,63,85},{220,224,63,85},{136,224,84,106},{94,182,42,42},{156,330,42,42}};
    const float layout[9][4]={{.38f,0,.24f,.17f},{.73f,.02f,.13f,.12f},{.87f,.02f,.13f,.12f},{.73f,.15f,.27f,.12f},
        {0,.3f,.28f,.4f},{.72f,.3f,.28f,.4f},{.3f,.22f,.4f,.52f},{0,.02f,.27f,.23f},{.37f,.77f,.26f,.2f}};
    for(int i=0;i<9;i++) {
        int xx,yy;
        if(side==1) {xx=target[i][0]+target[i][2]/2;yy=target[i][1]+target[i][3]/2+41;}
        else {xx=coords[i][0]+coords[i][2]/2+(mode==INVEN_UI_MODE_INVENTORY || mode>=INVEN_UI_MODE_NPC_IDENTIFY?0:345);yy=coords[i][1]+coords[i][3]/2+41;}
        SDL_FRect slot={r.x+layout[i][0]*r.w,r.y+layout[i][1]*r.h,layout[i][2]*r.w,layout[i][3]*r.h};
        fill(slot,(SDL_Color){20,16,12,255});outline(slot,(SDL_Color){130,100,57,255});
        panel_region(slot,xx,yy,0,0,2);
        int64_t obj=modern_inventory_at(xx,yy);
        if(obj && obj!=inven_ui_drag_item_obj_get()) {
            if(inside(slot,mouse_x,mouse_y)) {
                inventory_hover=obj;fill(slot,(SDL_Color){104,61,21,215});outline(slot,GOLD);
            }
            slot.x+=8;slot.y+=8;slot.w-=16;slot.h-=16;
            draw_art_id(obj_field_int32_get(obj,OBJ_F_ITEM_INV_AID),slot,true);
        } else centered(0,"·",slot.x+slot.w/2,slot.y+slot.h/2,MUTED);
    }
}
static void inventory_detail_update(int64_t pc,int64_t object,int kind)
{
    if(object!=inventory_detail_object || kind!=inventory_detail_kind)inventory_detail_scroll=inventory_compare_scroll=0;
    inventory_detail_object=object;inventory_detail_kind=kind;
    modern_inventory_details(pc,object,kind,&inventory_details);
}
static int inventory_detail_card(SDL_FRect r,ModernDetails* details,int scroll,int action)
{
    fill(r,(SDL_Color){19,16,12,245});outline(r,(SDL_Color){130,100,57,255});
    if(!details->count)return 0;
    if(details->icon>=0)draw_art(details->icon,(SDL_FRect){r.x+18,r.y+18,76,76},true);
    menu_paragraph(game_text(details->lines[0]),(SDL_FRect){r.x+110,r.y+20,r.w-132,81},0,GOLD);
    rule(r.x+18,r.y+110,r.w-36,MUTED);
    char body[40000]={0};
    for(int i=1;i<details->count;i++) {
        SDL_strlcat(body,game_text(details->lines[i]),sizeof(body));
        SDL_strlcat(body,"\n",sizeof(body));
    }
    SDL_FRect content={r.x+20,r.y+126,r.w-58,r.h-150};
    int rows=menu_paragraph(body,(SDL_FRect){0,0,content.w,0},0,GOLD);
    int maximum=rows-(int)(content.h/27);if(maximum<0)maximum=0;
    if(scroll>maximum)scroll=maximum;
    menu_paragraph(body,content,scroll,GOLD);
    if(maximum) {
        menu_button((SDL_FRect){r.x+r.w-36,r.y+125,28,38},action,"^",scroll>0);
        menu_button((SDL_FRect){r.x+r.w-36,r.y+r.h-55,28,38},action+1,"v",scroll<maximum);
    }
    return maximum;
}
static void draw_inventory(void)
{
    ModernInventory inv;if(!modern_inventory_state(&inv))return;
    panel_backdrop();inventory_hover=0;
    float w=fminf(1430,canvas_w-40),x=(canvas_w-w)/2,y=(canvas_h-820)/2;
    frame((SDL_FRect){x,y,w,820});
    centered(3,actor_name(inv.pc),canvas_w/2,y+22,GOLD);
    bool single=inv.mode==INVEN_UI_MODE_INVENTORY || inv.mode>=INVEN_UI_MODE_NPC_IDENTIFY;
    float left=x+25,right=x+440,cw=360;
    inventory_detail_rect=(SDL_FRect){x+830,y+182,w-855,548};
    SDL_FRect query={x+830,y+83,w-855,34};
    button(query,2400,"",-1,inventory_searching,true);
    text_compact(*inventory_query?inventory_query:tr("Rechercher un objet...","Find an item..."),query.x+10,query.y+8,query.w-20,false,GOLD);
    const char* filters[]={tr("Tous","All"),tr("Armes","Weapons"),tr("Armures","Armor"),tr("Consomm.","Consumables"),tr("Autres","Other")};
    for(int i=0;i<5;i++)button((SDL_FRect){query.x+i*(query.w+4)/5,y+127,(query.w-16)/5,34},2401+i,filters[i],-1,inventory_filter==i,true);
    button((SDL_FRect){right,y+688,cw,32},2406,tr("Comparer l'équipement","Compare equipment"),-1,inventory_compare,true);
    char info[320];snprintf(info,sizeof(info),"%s : %d / %d   ·   %s : %d",game_text(modern_inventory_text(9)),item_total_weight(inv.pc),stat_level_get(inv.pc,STAT_CARRY_WEIGHT),game_text(stat_name(STAT_SPEED)),stat_level_get(inv.pc,STAT_SPEED));
    text_draw(0,info,right,y+93,MUTED);
    char gold[80];snprintf(gold,sizeof(gold),"%d",item_gold_get(inv.pc));
    int64_t gold_obj=item_find_first_of_type(inv.pc,OBJ_TYPE_GOLD);
    if(gold_obj)draw_art_id(obj_field_int32_get(gold_obj,OBJ_F_ITEM_INV_AID),(SDL_FRect){x+w-165,y+24,40,32},true);
    text_draw(1,gold,x+w-116,y+28,GOLD);
    if(single) {
        draw_portrait(portrait_get(inv.pc),(SDL_FRect){left+cw/2-35,y+83,70,80});
        equipment_slots((SDL_FRect){left,y+182,cw,422},0,inv.mode);
    } else {
        text_draw(2,actor_name(inv.target),left,y+85,GOLD);
        text_in_button((SDL_FRect){left,y+126,250,52},734,game_text(modern_intgame_text(inv.target_paperdoll?1006:1005)),false,true);
        if(inv.target_paperdoll)equipment_slots((SDL_FRect){left,y+190,cw,420},1,inv.mode);
        else inventory_grid((SDL_FRect){left,y+190,cw,0},8,inv.mode==INVEN_UI_MODE_BARTER?209:177,inv.rows,inv.scroll);
        menu_button((SDL_FRect){left+cw+8,y+195,42,46},735,"^",inv.scroll>0);
        menu_button((SDL_FRect){left+cw+8,y+250,42,46},736,"v",true);
        if(inv.feedback && *inv.feedback)menu_paragraph(game_text(inv.feedback),(SDL_FRect){right,y+690,cw,48},0,GOLD);
        text_in_button((SDL_FRect){right,y+126,250,52},733,game_text(modern_intgame_text(inv.paperdoll?1006:1005)),false,true);
    }
    if(inv.paperdoll && !single)equipment_slots((SDL_FRect){right,y+190,cw,420},0,inv.mode);
    else inventory_grid((SDL_FRect){right,y+180,cw,0},368,49,12,0);
    /* Drag targets retain the engine's use/drop/transaction checks. */
    int targets[3][3]={{inv.drop,inv.drop_x,inv.drop_y},{inv.use,inv.use_x,inv.use_y},{inv.gamble,inv.gamble_x,inv.gamble_y}};
    const int icons[3]={224,342,345};
    for(int i=0;i<3;i++)if(targets[i][0]) {
        float gap=14,tw=(cw-2*gap)/3;
        SDL_FRect target={left+i*(tw+gap),y+651,tw,tw*292/488};
        if(inventory_actions_texture) {
            /* User-supplied atlas. Sample only the brass oval and its mounting
               plate, excluding the outer presentation frames. No extra frame. */
            float aw,ah;SDL_GetTextureSize(inventory_actions_texture,&aw,&ah);
            const float atlas_x[]={90,782,1474};
            SDL_FRect src={atlas_x[i]*aw/2048,187*ah/683,488*aw/2048,292*ah/683};
            bool hover=inside(target,mouse_x,mouse_y);
            Uint8 shade=hover?255:232;
            SDL_SetTextureColorMod(inventory_actions_texture,shade,shade,shade);
            SDL_RenderTexture(renderer,inventory_actions_texture,&src,&target);
        } else draw_art(icons[i],target,true);
        panel_region(target,targets[i][1],targets[i][2],0,0,2);
    }
    /* Recreate the original scattered-items → grid symbol at native size. */
    button((SDL_FRect){left,y+748,120,48},731,"",-1,false,true);
    const int scattered[6][2]={{0,8},{15,0},{25,19},{3,27},{22,33},{33,6}};
    for(int i=0;i<6;i++)fill((SDL_FRect){left+10+scattered[i][0],y+754+scattered[i][1],6,6},MUTED);
    if(!control_symbol((SDL_FRect){left+51,y+756,30,30},">",true,false))
        text_draw(2,">",left+55,y+759,(SDL_Color){190,69,35,255});
    for(int row=0;row<3;row++)for(int col=0;col<3;col++)
        fill((SDL_FRect){left+81+col*9,y+759+row*9,7,7},GOLD);
    for(int i=0;i<2;i++) {
        SDL_FRect stat={right+i*185,y+630,175,48};
        char label[256];snprintf(label,sizeof(label),"%s : %d",game_text(modern_intgame_text(i?63:57)),i?item_total_defence(inv.pc):item_total_attack(inv.pc));
        text_in_button(stat,738+i,label,false,true);
        if(inside(stat,mouse_x,mouse_y))inventory_detail_update(inv.pc,inv.pc,i+1);
    }
    if(inventory_hover)inventory_detail_update(inv.pc,inventory_hover,0);
    else if(!inventory_details.count)inventory_detail_update(inv.pc,inv.pc,1);
    int64_t equipped=0;
    if(inventory_compare && inventory_detail_kind==0 && inventory_detail_object && obj_handle_is_valid(inventory_detail_object)) {
        int slot=item_location_get(inventory_detail_object);
        if(slot>=FIRST_WEAR_INV_LOC && slot<=LAST_WEAR_INV_LOC)equipped=item_wield_get(inv.pc,slot);
        if(equipped==inventory_detail_object)equipped=0;
    }
    SDL_FRect primary=inventory_detail_rect;inventory_compare_rect=(SDL_FRect){0};
    if(equipped)primary.h=(primary.h-34)/2;
    inventory_detail_max=inventory_detail_card(primary,&inventory_details,inventory_detail_scroll,770);
    if(inventory_detail_scroll>inventory_detail_max)inventory_detail_scroll=inventory_detail_max;
    if(equipped){
        ModernDetails comparison;modern_inventory_details(inv.pc,equipped,0,&comparison);
        inventory_compare_rect=(SDL_FRect){primary.x,primary.y+primary.h+34,primary.w,primary.h};
        text_draw(0,tr("Actuellement équipé","Currently equipped"),primary.x+8,primary.y+primary.h+9,GOLD);
        inventory_compare_max=inventory_detail_card(inventory_compare_rect,&comparison,inventory_compare_scroll,776);
        if(inventory_compare_scroll>inventory_compare_max)inventory_compare_scroll=inventory_compare_max;
    }
    if(inv.mode==INVEN_UI_MODE_BARTER || inv.mode==INVEN_UI_MODE_LOOT)text_compact(tr("Maj + clic : transférer la pile","Shift + click: transfer stack"),left,y+722,790,false,MUTED);
    if(mainmenu_ui_is_active())menu_button((SDL_FRect){left+140,y+748,220,48},737,menu_text(7002),true);
    if(inv.mode==INVEN_UI_MODE_LOOT)menu_button((SDL_FRect){left+140,y+748,190,48},732,menu_text(260),true);
    menu_button((SDL_FRect){x+w-275,y+748,240,48},730,menu_text(260),true);
    if(*message_text && SDL_GetTicks()-message_time<12000)
        menu_paragraph(message_text,(SDL_FRect){left+290,y+744,w-610,54},0,GOLD);
    int64_t drag=inven_ui_drag_item_obj_get();
    if(drag)draw_art_id(obj_field_int32_get(drag,OBJ_F_ITEM_INV_AID),(SDL_FRect){mouse_x+12,mouse_y+12,72,88},true);
    if(intgame_mode_get()==INTGAME_MODE_QUANTITY) {
        int amount,max;modern_quantity_state(&amount,&max);
        float qx=canvas_w/2-280,qy=canvas_h/2-110;
        fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){0,0,0,150});frame((SDL_FRect){qx,qy,560,220});
        button_count=0;panel_region_count=0;
        char number[80];snprintf(number,sizeof(number),"%d / %d",amount,max);centered(3,number,canvas_w/2,qy+30,GOLD);
        menu_button((SDL_FRect){qx+25,qy+30,65,48},760,"-",amount>0);
        menu_button((SDL_FRect){qx+470,qy+30,65,48},761,"+",amount<max);
        menu_button((SDL_FRect){qx+190,qy+87,180,40},762,">>",true);
        menu_button((SDL_FRect){qx+25,qy+145,240,50},763,menu_text(260),amount>0);
        menu_button((SDL_FRect){qx+295,qy+145,240,50},764,menu_text(54),true);
    }
}
/* Font 744 is the game's book illustration atlas, not a text alphabet. */
static void written_book_picture(const char* text,SDL_FRect rect,int offset,int font)
{
    tig_art_id_t id;
    if(!*text || tig_art_interface_id_create(font,0,0,0,&id)!=TIG_OK)return;
    ArtTexture* art=art_get(tig_art_id_frame_set(id,(unsigned char)*text-31));
    if(!art || !art->texture)return;
    float scale=fminf(rect.w/art->width,162.0f/art->height);
    SDL_FRect dst={rect.x+(rect.w-art->width*scale)/2,rect.y-offset*27+(162-art->height*scale)/2,art->width*scale,art->height*scale};
    float top=fmaxf(dst.y,rect.y),bottom=fminf(dst.y+dst.h,rect.y+rect.h);
    if(bottom<=top)return;
    SDL_FRect src={0,(top-dst.y)/scale,art->width,(bottom-top)/scale};
    dst.y=top;dst.h=bottom-top;
    SDL_SetTextureColorMod(art->texture,INK.r,INK.g,INK.b);
    SDL_RenderTexture(renderer,art->texture,&src,&dst);
    SDL_SetTextureColorMod(art->texture,255,255,255);
}
static int written_document_page(SDL_FRect rect,int offset,SDL_Color color)
{
    int row=0,visible_rows=(int)(rect.h/27);
    for(int i=0;i<modern_written_paragraph_count();i++) {
        bool centered_text=false;int font=0;
        const char* source=modern_written_book_paragraph(i,&centered_text,&font);
        if(!source)continue;
        const char* text=game_text(source);
        int rows=(font==744 || font==488)?6:menu_paragraph(text,(SDL_FRect){0,0,rect.w,0},0,INK);
        int skip=offset>row?offset-row:0,start=row>offset?row-offset:0;
        if(start<visible_rows && skip<rows) {
            SDL_FRect part={rect.x,rect.y+start*27,rect.w,(visible_rows-start)*27};
            if(font==744 || font==488)written_book_picture(source,part,skip,font);
            else menu_paragraph_aligned(text,part,skip,color,centered_text);
        }
        row+=rows+1;
    }
    return row>0?row-1:0;
}
static int written_book_page(SDL_FRect rect,int offset)
{return written_document_page(rect,offset,INK);}
static void draw_written_document(void)
{
    bool news=modern_written_kind()==WRITTEN_TYPE_NEWSPAPER;
    bool plaque=modern_written_kind()==WRITTEN_TYPE_PLAQUE;
    float w=fminf(news?1300:920,canvas_w-80),h=canvas_h-110,x=(canvas_w-w)/2,y=30;
    SDL_Color ink=plaque?(SDL_Color){223,219,203,255}:INK;
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){10,8,6,150});
    frame((SDL_FRect){x-12,y-12,w+24,h+74});
    if(plaque){
        fill((SDL_FRect){x,y,w,h},(SDL_Color){36,40,41,255});
        outline((SDL_FRect){x+10,y+10,w-20,h-20},(SDL_Color){127,132,129,255});
        outline((SDL_FRect){x+14,y+14,w-28,h-28},(SDL_Color){61,65,64,255});
    }else material(paper_texture,(SDL_FRect){x,y,w,h});
    float top=35;
    if(news){
        /* Masthead uses installed translations, drawn in ink on paper. */
        centered(2,game_text(modern_written_heading(0)),x+w/2,y+24,INK);
        menu_paragraph_aligned(game_text(modern_written_heading(1)),(SDL_FRect){x+100,y+72,w-200,54},0,INK,true);
        text_draw(0,game_text(modern_written_heading(2)),x+30,y+18,INK);
        const char* exclusive=game_text(modern_written_heading(3));
        text_draw(0,exclusive,x+w-30-text_measure(0,exclusive),y+18,INK);
        rule(x+30,y+130,w-60,INK);top=150;
    }
    int columns=news?3:1;
    float column_width=(w-80-(columns-1)*44)/columns;
    SDL_FRect page={x+40,y+top,column_width,h-top-25};
    int rows=(int)(page.h/27);if(rows<1)rows=1;
    written_step=rows*columns;
    int total=written_document_page((SDL_FRect){0,0,column_width,0},0,ink);
    written_max=total>0?((total-1)/written_step)*written_step:0;
    if(written_scroll>written_max)written_scroll=written_max;
    written_scroll=(written_scroll/written_step)*written_step;
    for(int i=0;i<columns;i++){
        written_document_page(page,written_scroll+i*rows,ink);
        if(i+1<columns)fill((SDL_FRect){page.x+page.w+21,page.y,1,page.h},(SDL_Color){108,94,68,180});
        page.x+=column_width+44;
    }
    menu_button((SDL_FRect){x+15,y+h+12,80,42},991,"<",written_scroll>0);
    menu_button((SDL_FRect){x+w-95,y+h+12,80,42},992,">",written_scroll<written_max);
    menu_button((SDL_FRect){x+w/2-115,y+h+12,230,42},990,menu_text(4003),true);
}
static void draw_written_book(void)
{
    float w=fminf(1280,canvas_w-80),h=canvas_h-110,x=(canvas_w-w)/2,y=30;
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){10,8,6,150});
    frame((SDL_FRect){x-12,y-12,w+24,h+74});
    material(paper_texture,(SDL_FRect){x,y,w/2-5,h});
    material(paper_texture,(SDL_FRect){x+w/2+5,y,w/2-5,h});
    fill((SDL_FRect){x+w/2-5,y,10,h},(SDL_Color){50,32,17,255});
    SDL_FRect page={x+40,y+35,w/2-80,h-85};
    int page_rows=(int)(page.h/27);
    written_step=page_rows*2;
    int total=written_book_page((SDL_FRect){0,0,page.w,0},0);
    written_max=total>0?((total-1)/written_step)*written_step:0;
    if(written_scroll>written_max)written_scroll=written_max;
    written_scroll=(written_scroll/written_step)*written_step;
    written_book_page(page,written_scroll);
    page.x+=w/2;written_book_page(page,written_scroll+page_rows);
    char number[24];
    for(int side=0;side<2;side++) {
        snprintf(number,sizeof(number),"- %d -",written_scroll/page_rows+side+1);
        centered(1,number,x+w*(side?0.75f:0.25f),y+h-35,INK);
    }
    menu_button((SDL_FRect){x+15,y+h+12,80,42},991,"<",written_scroll>0);
    menu_button((SDL_FRect){x+w-95,y+h+12,80,42},992,">",written_scroll<written_max);
    menu_button((SDL_FRect){x+w/2-115,y+h+12,230,42},990,menu_text(4003),true);
}
static void draw_written(void)
{
    if(modern_written_is_book()){draw_written_book();return;}
    if(modern_written_kind()!=WRITTEN_TYPE_NOTE){draw_written_document();return;}
    written_step=3;
    const char* text=game_text(modern_written_text());
    float w=fminf(880,canvas_w-80),h=canvas_h-90,x=(canvas_w-w)/2,y=30;
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){10,8,6,150});
    frame((SDL_FRect){x-12,y-12,w+24,h+24});
    material(paper_texture,(SDL_FRect){x,y,w,h-66});
    SDL_FRect content={x+55,y+38,w-110,h-145};
    int rows=menu_paragraph(text,(SDL_FRect){0,0,content.w,0},0,INK);
    written_max=rows-(int)(content.h/27);if(written_max<0)written_max=0;
    if(written_scroll>written_max)written_scroll=written_max;
    menu_paragraph(text,content,written_scroll,INK);
    if(written_max) {
        menu_button((SDL_FRect){x+w-48,y+20,36,42},991,"^",written_scroll>0);
        menu_button((SDL_FRect){x+w-48,y+h-120,36,42},992,"v",written_scroll<written_max);
    }
    menu_button((SDL_FRect){x+w-250,y+h-55,230,45},990,menu_text(4003),true);
}
static SDL_FRect memo_map_rect;
static ModernMap memo_map_state;
static void draw_map_memos(ModernMap* map,SDL_FRect dest,float scale)
{
    notebook_sync();memo_map_rect=dest;memo_map_state=*map;
    if(memos_visible)for(int i=0;i<map_memo_count;i++) {
        MapMemo* memo=&map_memos[i];if(memo->mode!=map->mode || memo->map!=map->map)continue;
        float x=dest.x+(memo->x-map->offset_x)*scale,y=dest.y+(memo->y-map->offset_y)*scale;
        if(x<dest.x+12 || y<dest.y+12 || x>dest.x+dest.w-12 || y>dest.y+dest.h-12)continue;
        SDL_FRect r={x-12,y-12,24,24};button(r,2200+i,"*",-1,false,true);
        if(inside(r,mouse_x,mouse_y)) {
            float width=fminf(420,text_measure(0,memo->text)+20),xx=fminf(canvas_w-width-10,x);
            fill((SDL_FRect){xx,y+17,width,32},(SDL_Color){19,14,10,240});
            text_compact(memo->text,xx+10,y+24,width-20,false,GOLD);
        }
    }
}
static bool memo_action(int action)
{
    if(action==2190){memo_placing=!memo_placing;return true;}
    if(action==2191){memos_visible=!memos_visible;return true;}
    if(action>=2200 && action<2200+map_memo_count){memo_draft=map_memos[action-2200];memo_focus(action-2200);return true;}
    if(action==2194){memo_close();return true;}
    if(memo_edit<0)return false;
    if(action==2192 && *memo_draft.text){
        if(memo_edit<map_memo_count)map_memos[memo_edit]=memo_draft;
        else if(map_memo_count<64)map_memos[map_memo_count++]=memo_draft;
        notebook_save();memo_close();return true;
    }
    if(action==2193 && memo_edit<map_memo_count){map_memos[memo_edit]=map_memos[--map_memo_count];notebook_save();memo_close();return true;}
    return false;
}
static void draw_memo_editor(void)
{
    if(memo_edit<0)return;
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){0,0,0,155});
    float x=canvas_w/2-300,y=canvas_h/2-110;
    frame((SDL_FRect){x,y,600,220});button_count=panel_region_count=0;
    text_draw(2,tr("Note personnelle","Personal note"),x+25,y+18,GOLD);
    fill((SDL_FRect){x+25,y+62,550,70},(SDL_Color){19,14,10,255});
    menu_paragraph(memo_draft.text,(SDL_FRect){x+35,y+68,530,60},0,GOLD);
    button((SDL_FRect){x+25,y+155,170,42},2192,menu_text(260),-1,false,*memo_draft.text!=0);
    button((SDL_FRect){x+215,y+155,170,42},2193,tr("Supprimer","Delete"),-1,false,memo_edit<map_memo_count);
    button((SDL_FRect){x+405,y+155,170,42},2194,menu_text(54),-1,false,true);
}
#include "modern_map_bitmap.inc"
static void draw_map(void)
{
    ModernMap map;if(!modern_map_state(&map))return;
    bool local=modern_map_local_state(&map_local);
    bool hd=map_cache_prepare(&map);
    modern_map_hd_enable(local);
    panel_backdrop();float w=fminf(1440,canvas_w-40),x=(canvas_w-w)/2,y=(canvas_h-820)/2;
    frame((SDL_FRect){x,y,w,820});
    centered(3,game_text(modern_map_text(map.mode==1?502:501)),canvas_w/2,y+22,GOLD);
    menu_button((SDL_FRect){x+30,y+78,260,48},741,game_text(modern_map_text(501)),true);
    menu_button((SDL_FRect){x+310,y+78,260,48},742,game_text(modern_map_text(502)),true);
    button((SDL_FRect){x+590,y+78,220,48},2190,tr("Ajouter une note","Add note"),-1,memo_placing,map_memo_count<64 && !map.moving);
    button((SDL_FRect){x+830,y+78,200,48},2191,tr("Notes visibles","Show notes"),-1,memos_visible,true);
    if(SDL_GetTicks()-map_texture_time>50 || map_texture_mode!=map.mode || map_texture_hd!=local || !map_texture) {
        float ww,hh;SDL_DestroyTexture(map_texture);
        map_texture=local?map_overlay_texture(map.buffer):texture_from_buffer(map.buffer,&ww,&hh);
        map_texture_time=SDL_GetTicks();map_texture_mode=map.mode;
        map_texture_hd=local;
    }
    SDL_FRect available={x+30,y+150,w-60,550};
    float scale=fminf(available.w/map.width,available.h/map.height);
    ModernMap native=map;
    map_camera_active=local;
    map_viewport=available;
    if(map_camera_active) {
        if(map_camera_id!=map.townmap) {
            map_camera=(MapCamera){
                .x=map.offset_x-map.origin_x+(map.width-fminf(map_local.width,available.w/scale))/2,
                .y=map.offset_y-map.origin_y+(map.height-fminf(map_local.height,available.h/scale))/2,.scale=scale};
            map_camera_id=map.townmap;
        }
        map_camera.min_scale=fminf(available.w/map_local.width,available.h/map_local.height);
        map_camera.max_scale=fmaxf(scale,hd && map_cache_info.scale>0?1/map_cache_info.scale:4);
        map_camera_zoom(&map_camera,1,0,0,available.w,available.h,map_local.width,map_local.height);
        scale=map_camera.scale;
        map.width=(int)ceilf(map_camera.width);map.height=(int)ceilf(map_camera.height);
        map.offset_x=(int)map_camera.x;map.offset_y=(int)map_camera.y;
        map.origin_x=map.origin_y=0;
    }
    SDL_FRect dest={available.x+(available.w-map.width*scale)/2,available.y+(available.h-map.height*scale)/2,map.width*scale,map.height*scale};
    map_destination=dest;
    SDL_FRect src={native.x,native.y-41,native.width,native.height};
    fill(available,(SDL_Color){12,10,8,255});
    if(local && (!hd || !map_cache_draw(&map,dest,scale)))map_bitmap_draw(&map,dest,scale);
    SDL_Rect saved_clip;bool clipped=SDL_RenderClipEnabled(renderer);
    SDL_GetRenderClipRect(renderer,&saved_clip);
    SDL_Rect clip={(int)dest.x,(int)dest.y,(int)dest.w,(int)dest.h};
    SDL_SetRenderClipRect(renderer,&clip);
    SDL_FRect overlay=dest;
    if(map_camera_active)overlay=(SDL_FRect){
        dest.x+(native.offset_x-native.origin_x-map.offset_x)*scale,
        dest.y+(native.offset_y-native.origin_y-map.offset_y)*scale,
        native.width*scale,native.height*scale};
    if(map_texture)SDL_RenderTexture(renderer,map_texture,&src,&overlay);
    if(local) {
        float origin_x=map_camera_active?0:map.origin_x,origin_y=map_camera_active?0:map.origin_y;
        for(int i=0;i<=map_local.count;i++) {
            float xx=dest.x+(map_local.x[i]+origin_x-map.offset_x)*scale;
            float yy=dest.y+(map_local.y[i]+origin_y-map.offset_y)*scale;
            if(i) {
                float previous_x=dest.x+(map_local.x[i-1]+origin_x-map.offset_x)*scale;
                float previous_y=dest.y+(map_local.y[i-1]+origin_y-map.offset_y)*scale;
                SDL_SetRenderDrawColor(renderer,235,193,100,255);
                SDL_RenderLine(renderer,previous_x,previous_y,xx,yy);
                SDL_RenderLine(renderer,previous_x,previous_y+1,xx,yy+1);
            }
            float radius=i==map_local.count && i?7:4;
            fill((SDL_FRect){xx-radius,yy-radius,radius*2,radius*2},i?GOLD:(SDL_Color){240,240,230,255});
            outline((SDL_FRect){xx-radius-2,yy-radius-2,radius*2+4,radius*2+4},GOLD);
        }
    }
    draw_map_memos(&map,dest,scale);
    SDL_SetRenderClipRect(renderer,clipped?&saved_clip:NULL);
    outline(dest,GOLD);
    panel_region(dest,map_camera_active?map.offset_x:map.x,map_camera_active?map.offset_y:map.y,map.width,map.height,3);
    if(local) {
        button((SDL_FRect){x+w-140,y+78,48,48},749,"-",-1,false,map_camera_active && map_camera.scale>map_camera.min_scale+0.001f);
        button((SDL_FRect){x+w-82,y+78,48,48},750,"+",-1,false,map_camera_active && map_camera.scale<map_camera.max_scale-0.001f);
        menu_button((SDL_FRect){x+w-585,y+741,220,50},748,menu_text(54),map_local.count>0);
    }
    menu_button((SDL_FRect){x+30,y+741,230,50},740,menu_text(4003),true);
    text_in_button((SDL_FRect){x+w-350,y+741,320,50},743,game_text(modern_map_text(512)),false,map.mode!=1);
    if(memo_placing)text_draw(0,tr("Cliquez sur la carte pour placer la note.","Click the map to place your note."),x+35,y+711,GOLD);
    else if(*message_text && SDL_GetTicks()-message_time<12000)menu_paragraph(message_text,(SDL_FRect){x+35,y+704,w-70,34},0,GOLD);
    draw_memo_editor();
}

static SDL_FRect dialogue_reply_view;
static int dialogue_reply_scroll,dialogue_reply_max;
static void dialogue_paragraph(const char* text,SDL_FRect rect,int offset,SDL_Color color)
{
    /* A thin text shadow complements the translucent dialogue backing. */
    SDL_FRect shadow=rect;shadow.x+=1.3f;shadow.y+=1.3f;
    menu_paragraph(text,shadow,offset,(SDL_Color){0,0,0,240});
    menu_paragraph(text,rect,offset,color);
}
static void dialogue_scroll_button(SDL_FRect rect,int action,const char* label,bool active)
{
    if(!active)return;
    if(!control_symbol(rect,label,true,inside(rect,mouse_x,mouse_y)))
        text_draw(1,label,rect.x+8,rect.y+4,GOLD);
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){rect,action,true};
}
static void draw_world_hover(void)
{
    if(journal || native_panel() || !visible())return;
    const ModernDetails* detail=modern_hover_details();if(!detail)return;
    float width=fminf(700,canvas_w-80),text_width=width-96;
    /* Pack the original fields into one wrapping row, preserving every value. */
    char description[40100]="";size_t used=0;
    for(int i=0;i<detail->count;i++) {
        if(!strcmp(detail->lines[i],detail->title))continue;
        bool gauge=false;
        for(int j=0;j<detail->bar_count;j++)if(detail->bar_lines[j]==i)gauge=true;
        if(gauge)continue;
        const char* separator=used?(detail->bar_count?"   |   ":"\n"):"";
        used+=(size_t)snprintf(description+used,sizeof(description)-used,"%s%s",separator,detail->lines[i]);
    }
    int title_rows=menu_paragraph(game_text(detail->title),(SDL_FRect){0,0,text_width,0},0,GOLD);
    int rows=*description?menu_paragraph(game_text(description),(SDL_FRect){0,0,text_width,0},0,GOLD):0;
    float height=20+(title_rows+rows)*27+(detail->bar_count?26:0)+(detail->extra_count?28:0);
    if(height<86)height=86;
    float x=(canvas_w-width)/2,y=canvas_h-136-height;
    fill((SDL_FRect){x,y,width,height},(SDL_Color){16,12,9,220});outline((SDL_FRect){x,y,width,height},MUTED);
    if(detail->portrait>=0)draw_portrait(detail->portrait,(SDL_FRect){x+10,y+10,56,65});
    else if(detail->icon>=0)draw_art(detail->icon,(SDL_FRect){x+10,y+10,56,56},true);
    float tx=x+78,ty=y+10;
    menu_paragraph(game_text(detail->title),(SDL_FRect){tx,ty,text_width,title_rows*27},0,GOLD);ty+=title_rows*27;
    if(rows) {
        menu_paragraph(game_text(description),(SDL_FRect){tx,ty,text_width,rows*27},0,GOLD);ty+=rows*27;
    }
    float column_width=(text_width-20*(detail->bar_count-1))/fmaxf(1,detail->bar_count);
    for(int j=0;j<detail->bar_count;j++) {
        int i=detail->bar_lines[j];if(i<0 || i>=detail->count)continue;
        int kind=detail->bar_kinds[j];SDL_Color c=kind==1?(SDL_Color){105,136,244,255}:kind==2?(SDL_Color){124,194,94,255}:(SDL_Color){216,65,53,255};
        const char* value=game_text(detail->lines[i]);float value_width=text_measure(1,value);
        float gx=tx+j*(column_width+20);
        SDL_FRect gauge={gx,ty+7,column_width-value_width-10,10};
        float fraction=fmaxf(0,fminf(1,detail->bar_values[j]/100.0f));
        fill(gauge,(SDL_Color){35,30,25,255});outline(gauge,MUTED);
        fill((SDL_FRect){gauge.x+2,gauge.y+2,(gauge.w-4)*fraction,gauge.h-4},c);
        text_draw(1,value,gx+column_width-value_width,ty,c);
    }
    if(detail->bar_count)ty+=26;
    for(int i=0;i<detail->extra_count;i++)draw_art(detail->extra_icons[i],(SDL_FRect){tx+i*28,ty,24,24},true);
}
static void draw_world_bubbles(void)
{
    if(journal || !visible())return;
    int panel=native_panel();if(panel && panel!=4 && panel!=8)return;
    ModernBubble bubbles[8];int count=modern_bubbles(bubbles,8);
    const SDL_Color colors[]={{235,229,207,255},{255,115,100,255},{136,229,138,255},{142,174,255,255}};
    float sx=canvas_w/hrp_iso_window_width_get(),sy=canvas_h/hrp_iso_window_height_get();
    for(int i=0;i<count;i++) {
        ModernBubble* b=&bubbles[i];const char* text=game_text(b->text);
        float width=fminf(420,fmaxf(180,text_measure(1,text)+8));
        int rows=menu_paragraph(text,(SDL_FRect){0,0,width,0},0,GOLD);
        float height=rows*27;
        /* Let unusually long script lines use more width instead of clipping. */
        while(height>canvas_h-250 && width<canvas_w-80) {
            width=fminf(canvas_w-80,width+120);
            rows=menu_paragraph(text,(SDL_FRect){0,0,width,0},0,GOLD);height=rows*27;
        }
        float x=(b->x+b->width/2.0f)*sx-width/2,y=(b->y+b->height)*sy-height;
        x=fmaxf(20,fminf(canvas_w-width-20,x));
        y=fmaxf(90,fminf(canvas_h-150-height,y));
        SDL_FRect rect={x,y,width,height};
        fill((SDL_FRect){x-10,y-7,width+20,height+14},(SDL_Color){12,10,8,160});
        dialogue_paragraph(text,rect,0,colors[b->type>=0 && b->type<4?b->type:0]);
    }
}
static void draw_dialogue(void)
{
    const DialogState* state=modern_dialog_state();if(!state)return;
    if(strcmp(dialogue_previous,state->reply)) {
        SDL_strlcpy(dialogue_previous,state->reply,sizeof(dialogue_previous));
        dialogue_scroll=dialogue_reply_scroll=0;dialogue_choice=-1;
    }
    float width=fminf(1000,canvas_w-100),x=(canvas_w-width)/2;
    float choices_height=0;
    for(int i=0;i<state->num_options && i<5;i++)
        choices_height+=menu_paragraph(game_text(state->options[i]),(SDL_FRect){0,0,width-48,0},0,GOLD)*27+12;
    dialogue_view=(SDL_FRect){x,canvas_h-345,width,fminf(195,choices_height)};
    /* Keep the scenery visible, with consistent contrast on bright terrain. */
    if(choices_height>0)fill((SDL_FRect){x-14,dialogue_view.y-10,width+28,dialogue_view.h+20},(SDL_Color){12,10,8,160});
    int ax=0,ay=0;modern_dialog_anchor(&ax,&ay);
    float rx=ax*canvas_w/hrp_iso_window_width_get()-190;
    float ry=ay*canvas_h/hrp_iso_window_height_get();
    const char* reply=game_text(state->reply);
    int rows=menu_paragraph(reply,(SDL_FRect){0,0,380,0},0,GOLD);
    float reply_height=fminf(243,rows*27);
    rx=fmaxf(25,fminf(canvas_w-405,rx));ry=fmaxf(85,fminf(dialogue_view.y-reply_height-24,ry));
    dialogue_reply_view=(SDL_FRect){rx,ry,380,reply_height};
    dialogue_reply_max=rows-(int)(reply_height/27);if(dialogue_reply_max<0)dialogue_reply_max=0;
    if(dialogue_reply_scroll>dialogue_reply_max)dialogue_reply_scroll=dialogue_reply_max;
    fill((SDL_FRect){rx-12,ry-10,404,reply_height+20},(SDL_Color){12,10,8,160});
    dialogue_paragraph(reply,dialogue_reply_view,dialogue_reply_scroll,(SDL_Color){245,237,216,255});
    dialogue_scroll_button((SDL_FRect){rx+382,ry,30,30},822,"^",dialogue_reply_scroll>0);
    dialogue_scroll_button((SDL_FRect){rx+382,ry+reply_height-30,30,30},823,"v",dialogue_reply_scroll<dialogue_reply_max);
    SDL_Rect clip={(int)dialogue_view.x,(int)dialogue_view.y,(int)dialogue_view.w,(int)dialogue_view.h};SDL_SetRenderClipRect(renderer,&clip);
    float cy=dialogue_view.y-dialogue_scroll;
    for(int i=0;i<state->num_options && i<5;i++) {
        const char* option=game_text(state->options[i]);
        int lines=menu_paragraph(option,(SDL_FRect){0,0,width-48,0},0,GOLD);
        float h=lines*27+12;SDL_FRect r={x,cy,width,h};
        bool hovered=inside(r,mouse_x,mouse_y) && inside(dialogue_view,mouse_x,mouse_y);
        SDL_Color color=hovered?(SDL_Color){255,222,110,255}:(SDL_Color){245,237,216,255};
        char number[8];snprintf(number,sizeof(number),"%d",i+1);
        dialogue_paragraph(number,(SDL_FRect){x,cy+4,28,27},0,color);
        dialogue_paragraph(option,(SDL_FRect){x+40,cy+4,width-48,h-8},0,color);
        float top=fmaxf(r.y,dialogue_view.y),bottom=fminf(r.y+r.h,dialogue_view.y+dialogue_view.h);
        if(bottom>top && button_count<MAX_BUTTONS)buttons[button_count++]=(Button){{r.x,top,r.w,bottom-top},800+i,true};
        cy+=h;
    }
    dialogue_max=(int)fmaxf(0,cy+dialogue_scroll-dialogue_view.y-dialogue_view.h);
    if(dialogue_scroll>dialogue_max)dialogue_scroll=dialogue_max;
    SDL_SetRenderClipRect(renderer,NULL);
    dialogue_scroll_button((SDL_FRect){x+width+8,dialogue_view.y,32,32},820,"^",dialogue_scroll>0);
    dialogue_scroll_button((SDL_FRect){x+width+8,dialogue_view.y+160,32,32},821,"v",dialogue_scroll<dialogue_max);
}

static void draw_selector(void)
{
    if(selector==1 && tactical_active()){draw_tactical_spells();return;}
    float w=fminf(1080,canvas_w-60),x=(canvas_w-w)/2,y=(canvas_h-700)/2;
    frame((SDL_FRect){x,y,w,700});
    centered(3,character_text(selector==1?134:132),canvas_w/2,y+25,GOLD);
    int64_t pc=combat_input_actor();
    if(selector==1) {
        for(int i=0;i<16;i++)text_in_button((SDL_FRect){x+25+(i%4)*(w-50)/4,y+85+(i/4)*50,(w-50)/4-6,44},850+i,game_text(spell_college_name(i)),selector_college==i,true);
        for(int i=0;i<5;i++) {
            int id=selector_college*5+i;float yy=y+303+i*59;
            button((SDL_FRect){x+25,yy,w-50,53},870+i,"",spell_icon(id),false,spell_is_known(pc,id));
            text_draw(1,game_text(spell_name(id)),x+87,yy+13,spell_is_known(pc,id)?GOLD:MUTED);
        }
    } else {
        for(int i=0;i<4;i++) {
            SDL_FRect r={x+30,y+95+i*110,w-60,90};
            button(r,880+i,"",sub_579F50(i),false,sub_579F50(i)!=-1);
            text_draw(2,game_text(sub_57A700(i)),r.x+95,r.y+28,GOLD);
        }
    }
    menu_button((SDL_FRect){x+w-260,y+625,230,50},840,menu_text(54),true);
}

static void draw_sleep(void)
{
    float x=canvas_w/2-380,y=(canvas_h-700)/2;
    frame((SDL_FRect){x,y,760,700});
    for(int i=0;i<modern_sleep_count();i++)
        text_in_button((SDL_FRect){x+30,y+30+i*66,700,58},900+i,game_text(modern_sleep_text(i)),false,true);
    menu_button((SDL_FRect){x+480,y+625,250,48},908,menu_text(54),true);
}
static void draw_schematic(void)
{
    ModernSchematic scheme;if(!modern_schematic_state(&scheme))return;
    panel_backdrop();float w=fminf(1360,canvas_w-40),x=(canvas_w-w)/2,y=(canvas_h-800)/2;
    frame((SDL_FRect){x,y,w,800});
    centered(3,character_text(133),canvas_w/2,y+24,GOLD);
    for(int i=0;i<8;i++)text_in_button((SDL_FRect){x+25+(i%4)*(w-50)/4,y+82+i/4*56,(w-50)/4-7,48},930+i,game_text(tech_discipline_name_get(i)),scheme.tech==i,true);
    for(int i=0;i<2;i++) {
        SDL_FRect r={x+25+i*(w-50)/2,y+205,(w-70)/2,120};
        button(r,923+i,"",-1,scheme.view==i,true);
        menu_paragraph(game_text(modern_schematic_text(4+i)),(SDL_FRect){r.x+16,r.y+12,r.w-32,r.h-18},0,GOLD);
    }
    char page[32];snprintf(page,sizeof(page),"%d / %d",scheme.count?scheme.page+1:0,scheme.count);
    centered(0,page,x+105,y+695,MUTED);
    if(scheme.id>=0) {
        text_draw(2,game_text(scheme.name),x+35,y+340,GOLD);
        draw_art(scheme.art,(SDL_FRect){x+35,y+389,340,200},true);
        description_panel(game_text(scheme.description),(SDL_FRect){x+405,y+389,w-440,200});
        int64_t components[]={scheme.component1,scheme.component2};
        for(int i=0;i<2;i++)if(components[i]) {
            draw_art_id(obj_field_int32_get(components[i],OBJ_F_ITEM_INV_AID),(SDL_FRect){x+405+i*340,y+605,64,64},true);
            menu_paragraph(actor_name(components[i]),(SDL_FRect){x+480+i*340,y+610,250,70},0,GOLD);
        }
    }
    menu_button((SDL_FRect){x+25,y+727,70,48},920,"<",scheme.page>0);
    menu_button((SDL_FRect){x+110,y+727,70,48},921,">",scheme.page+1<scheme.count);
    SDL_FRect combine={x+210,y+717,75,65};
    button(combine,922,"",357,false,scheme.id>=0);
    if(scheme.id>=0 && scheme.readiness>=0 && scheme.readiness<3)draw_art(360-scheme.readiness,(SDL_FRect){combine.x+12,combine.y+8,51,49},true);
    if(inside(combine,mouse_x,mouse_y))menu_paragraph(game_text(modern_schematic_text(3)),(SDL_FRect){x+305,y+708,w-600,81},0,GOLD);
    menu_button((SDL_FRect){x+w-250,y+727,225,48},929,menu_text(260),true);
    if(*message_text)menu_paragraph(message_text,(SDL_FRect){x+35,y+680,w-70,40},0,GOLD);
}
static void draw_fate(void)
{
    int count=modern_fate_count();float w=fminf(810,canvas_w-60),x=(canvas_w-w)/2,y=88;
    frame((SDL_FRect){x,y,w,646});
    char title[256];snprintf(title,sizeof(title),"%s : %d",game_text(stat_name(STAT_FATE_POINTS)),stat_level_get(player_get_local_pc_obj(),STAT_FATE_POINTS));
    centered(2,title,canvas_w/2,y+20,GOLD);
    for(int i=0;i<count;i++) {
        SDL_FRect r={x+24,y+68+i*40,w-48,36};
        button(r,970+i,"",-1,modern_fate_active(i),true);
        text_draw(1,game_text(modern_fate_text(i)),r.x+18,r.y+5,GOLD);
        if(modern_fate_active(i) && !control_symbol((SDL_FRect){r.x+r.w-30,r.y+4,24,24},"+",true,true))
            text_draw(1,"+",r.x+r.w-25,r.y+5,GOLD);
    }
    menu_button((SDL_FRect){x+w-250,y+578,225,46},969,menu_text(260),true);
}
static void draw_play_panels(void)
{
    panel_region_count=0;
    switch(native_panel()) {
    case 1:panel_backdrop();draw_character_sheet();break;
    case 2:draw_inventory();break;
    case 9:draw_written();break;
    case 3:draw_map();break;
    case 4:draw_dialogue();break;
    case 5:draw_selector();break;
    case 6:draw_sleep();break;
    case 7:draw_schematic();break;
    case 8:draw_fate();break;
    }
}
static bool panels_action(int action)
{
    int p=native_panel();
    if(p==9 && action>=990 && action<=992) {
        if(action==990)written_ui_close();
        else {written_scroll+=action==991?-written_step:written_step;if(written_scroll<0)written_scroll=0;if(written_scroll>written_max)written_scroll=written_max;}
        return true;
    }
    if(action==950 || action==951) {
        if(!p && (intgame_mode_get()==INTGAME_MODE_MAIN || intgame_mode_get()==INTGAME_MODE_SKILL)) {
            if(action==950 && intgame_mode_get()==INTGAME_MODE_SKILL && sub_579F90()==1)skill_ui_cancel();
            else sub_579FA0(combat_input_actor(),action==950?1:0);
        }
        return true;
    }
    if(p==8 && action>=969 && action<982){modern_fate_choose(action-970);return true;}
    if(p==3 && memo_action(action))return true;
    if(p==2 && action>=2400 && action<=2406){
        if(action==2400)inventory_search_focus(!inventory_searching);
        else if(action==2406){inventory_compare=!inventory_compare;inventory_compare_scroll=0;}
        else inventory_filter=action-2401;
        return true;
    }
    if(p==2 && (action==776 || action==777)){
        inventory_compare_scroll+=action==776?-3:3;
        if(inventory_compare_scroll<0)inventory_compare_scroll=0;
        if(inventory_compare_scroll>inventory_compare_max)inventory_compare_scroll=inventory_compare_max;
        return true;
    }
    if(p==2 && (action==738 || action==739)) {
        ModernInventory inv;if(modern_inventory_state(&inv))inventory_detail_update(inv.pc,inv.pc,action-737);return true;
    }
    if(p==2 && (action==770 || action==771)) {
        inventory_detail_scroll+=action==770?-3:3;
        if(inventory_detail_scroll<0)inventory_detail_scroll=0;
        if(inventory_detail_scroll>inventory_detail_max)inventory_detail_scroll=inventory_detail_max;return true;
    }
    if(action>=900 && action<=908 && p==6){modern_sleep_choose(action==908?-1:action-900);return true;}
    if(action>=920 && action<=937 && p==7){modern_schematic_action(action==929?-1:action-920);description_scroll=0;return true;}
    if(action==5 || action==6) {
        if(modern_ui_open_selector(action==6))return true;
    }
    if(p==1 && ((action>=600 && action<700) || action==540 || action==541))return creation_menu_action(action);
    if(action==720 && p==1){charedit_close();return true;}
    if(action==737 && p==2 && mainmenu_ui_is_active()){modern_mainmenu_activate(1);return true;}
    if(action==730 && p==2 && mainmenu_ui_is_active()){modern_mainmenu_activate(0);return true;}
    if(action>=730 && action<=736 && p==2) {
        if(action>=735)modern_inventory_scroll(action==735?-1:1);
        else modern_inventory_action(action-730);
        return true;
    }
    if(p==3 && action==748){modern_map_route_clear();map_texture_time=0;return true;}
    if(p==3 && (action==749 || action==750)){map_zoom(action==749?0.8f:1.25f,map_destination.x+map_destination.w/2,map_destination.y+map_destination.h/2);return true;}
    if(action>=740 && action<=747 && p==3){modern_map_action(action-740);return true;}
    if(action>=760 && action<=764 && p==2) {
        int amount,max;modern_quantity_state(&amount,&max);
        modern_quantity_action(action==764?-1:action==762?max:amount+(action==760?-1:action==761?1:0),action==763);return true;
    }
    if(action>=800 && action<805 && p==4){modern_dialog_choose(action-800);return true;}
    if(action==820 || action==821){dialogue_scroll+=action==820?-81:81;if(dialogue_scroll<0)dialogue_scroll=0;if(dialogue_scroll>dialogue_max)dialogue_scroll=dialogue_max;return true;}
    if(action==822 || action==823){dialogue_reply_scroll+=action==822?-3:3;if(dialogue_reply_scroll<0)dialogue_reply_scroll=0;if(dialogue_reply_scroll>dialogue_reply_max)dialogue_reply_scroll=dialogue_reply_max;return true;}
    if(action==840){selector=0;return true;}
    if(action>=850 && action<866){selector_college=action-850;return true;}
    if(action>=870 && action<875 && p==5) {
        int spl=selector_college*5+action-870;
        if(spell_is_known(combat_input_actor(),spl)){selector=0;spell_ui_activate(combat_input_actor(),spl);}return true;
    }
    if(action>=880 && action<884 && p==5){selector=0;sub_579FA0(combat_input_actor(),action-880);return true;}
    return false;
}
static void panels_tick(void)
{
    modern_map_route_tick();
    int p=native_panel();
    if(p!=panel_previous) {
        button_count=0;panel_region_count=0;pressed_action=-1;click_captured=false;
        if(memo_edit>=0)memo_close();memo_placing=false;
        if(inventory_searching)inventory_search_focus(false);
        inventory_quick_release=false;
        if(p==9)written_scroll=0;
        if(p==1){sheet_category=1;sheet_page=sheet_selected=0;sheet_select_detail(1,0);}
        if(p!=5)selector=0;
        binding_cancel=binding_pressed;binding_pressed=binding_moved=false;binding_bar_visible=false;
        map_pressed=map_dragged=false;map_pan_x=map_pan_y=0;map_camera_id=-1;map_camera_active=false;map_bitmap_clear();SDL_CaptureMouse(false);
        if(p==2){inventory_query[0]=0;inventory_filter=0;inventory_compare_scroll=0;memset(&inventory_details,0,sizeof(inventory_details));inventory_detail_object=0;inventory_detail_scroll=0;}
        panel_previous=p;
    }
    if(p==3 && ((int)map_pan_x || (int)map_pan_y)) {
        int dx=(int)map_pan_x,dy=(int)map_pan_y;
        if(map_camera_active){map_camera.x+=dx;map_camera.y+=dy;map_camera_clamp(&map_camera,map_local.width,map_local.height);}
        else modern_map_pan(dx,dy);
        map_pan_x-=dx;map_pan_y-=dy;map_texture_time=0;
    }
    for(int i=0;i<panel_event_count;i++) {
        if(p==2 && panel_events[i].kind==4){
            modern_inventory_mouse(panel_events[i].x,panel_events[i].y,TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP);
            if(intgame_mode_get()==INTGAME_MODE_QUANTITY){int value,maximum;modern_quantity_state(&value,&maximum);modern_quantity_action(maximum,true);}
        }
        if(panel_events[i].kind==p) {
            if(p==2)modern_inventory_mouse(panel_events[i].x,panel_events[i].y,panel_events[i].event);
            if(p==3){
                if(map_camera_active)modern_map_local_mouse(panel_events[i].x,panel_events[i].y,panel_events[i].event,(int)ceilf(10/map_camera.scale));
                else modern_map_mouse(panel_events[i].x,panel_events[i].y,panel_events[i].event);
            }
        }
    }
    panel_event_count=0;
    binding_tick();
}
/* Delay route placement until release; crossing six logical pixels starts a pan. */
static bool map_drag_event(int type,int button,PanelRegion* region)
{
    bool motion=type==SDL_EVENT_MOUSE_MOTION;
    if(map_pressed) {
        if(motion) {
            float dx=mouse_x-map_start_x,dy=mouse_y-map_start_y;
            if(!map_dragged && dx*dx+dy*dy>=36)map_dragged=true;
            if(map_dragged) {
                map_pan_x+=(map_last_x-mouse_x)*map_drag_region.w/map_drag_region.rect.w;
                map_pan_y+=(map_last_y-mouse_y)*map_drag_region.h/map_drag_region.rect.h;
                map_last_x=mouse_x;map_last_y=mouse_y;
            }
        } else if(type==SDL_EVENT_MOUSE_BUTTON_UP && button==SDL_BUTTON_LEFT) {
            if(!map_dragged && inside(map_drag_region.rect,mouse_x,mouse_y)) {
                panel_mouse_queue(3,map_press_x,map_press_y,TIG_MESSAGE_MOUSE_MOVE);
                panel_mouse_queue(3,map_press_x,map_press_y,TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN);
                panel_mouse_queue(3,map_press_x,map_press_y,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP);
            }
            map_pressed=false;SDL_CaptureMouse(false);
        }
        return true;
    }
    if(region && type==SDL_EVENT_MOUSE_BUTTON_DOWN && button==SDL_BUTTON_LEFT) {
        map_pressed=true;map_dragged=false;map_drag_region=*region;
        map_start_x=map_last_x=mouse_x;map_start_y=map_last_y=mouse_y;
        map_press_x=region->x+(mouse_x-region->rect.x)*region->w/region->rect.w;
        map_press_y=region->y+(mouse_y-region->rect.y)*region->h/region->rect.h;
        SDL_CaptureMouse(true);return true;
    }
    return false;
}
static bool panels_event(SDL_Event* event)
{
    int p=native_panel();if(!p)return false;
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST) {
        binding_event(event);inventory_quick_release=false;
        map_pressed=map_dragged=false;map_pan_x=map_pan_y=0;SDL_CaptureMouse(false);return false;
    }
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);if(ww<=0 || wh<=0)return false;
    if(event->type==SDL_EVENT_MOUSE_MOTION){mouse_x=event->motion.x*canvas_w/ww;mouse_y=event->motion.y*canvas_h/wh;}
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP){mouse_x=event->button.x*canvas_w/ww;mouse_y=event->button.y*canvas_h/wh;}
    if(p==3 && memo_event(event))return true;
    if(p==3 && memo_edit<0 && (event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP)) {
        int action=hit(mouse_x,mouse_y);
        if(action>=2200 && action<2264){
            if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN)pressed_action=action;
            else if(pressed_action==action){pending_action=action;pressed_action=-1;}
            return true;
        }
        if(memo_placing && inside(memo_map_rect,mouse_x,mouse_y)){
            if(event->type==SDL_EVENT_MOUSE_BUTTON_UP && event->button.button==SDL_BUTTON_LEFT){
                memo_draft=(MapMemo){.mode=memo_map_state.mode,.map=memo_map_state.map,
                    .x=memo_map_state.offset_x+(mouse_x-memo_map_rect.x)*memo_map_state.width/memo_map_rect.w,
                    .y=memo_map_state.offset_y+(mouse_y-memo_map_rect.y)*memo_map_state.height/memo_map_rect.h};
                memo_placing=false;memo_focus(64);
            }return true;
        }
    }
    if(p==2 && inventory_search_event(event))return true;
    if(p==2 && inventory_quick_release && event->type==SDL_EVENT_MOUSE_BUTTON_UP && event->button.button==SDL_BUTTON_LEFT){
        inventory_quick_release=false;
        if(inside(inventory_quick_region,mouse_x,mouse_y) && modern_inventory_at(inventory_quick_x,inventory_quick_y)==inventory_quick_item)
            panel_mouse_queue(4,inventory_quick_x,inventory_quick_y,0);
        return true;
    }
    if(p==2 && event->type==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button==SDL_BUTTON_LEFT && (SDL_GetModState()&SDL_KMOD_SHIFT)){
        ModernInventory inv;
        if(modern_inventory_state(&inv) && (inv.mode==INVEN_UI_MODE_BARTER || inv.mode==INVEN_UI_MODE_LOOT))
        for(int i=0;i<panel_region_count;i++){
            PanelRegion* r=&panel_regions[i];if(r->kind!=2 || !inside(r->rect,mouse_x,mouse_y))continue;
            int xx=r->x+(mouse_x-r->rect.x)*r->w/r->rect.w,yy=r->y+(mouse_y-r->rect.y)*r->h/r->rect.h;
            if(inventory_match(modern_inventory_at(xx,yy))){
                inventory_quick_release=true;inventory_quick_x=xx;inventory_quick_y=yy;
                inventory_quick_item=modern_inventory_at(xx,yy);
                inventory_quick_region=(SDL_FRect){mouse_x-5,mouse_y-5,10,10};return true;
            }
        }
    }
    if((p==2 || p==5) && binding_event(event))return true;
    if(event->type==SDL_EVENT_MOUSE_WHEEL) {
        if(p==3 && inside(map_destination,mouse_x,mouse_y)){
            float amount=event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED?-event->wheel.y:event->wheel.y;
            if(amount)map_zoom(powf(1.25f,amount),mouse_x,mouse_y);
        }
        else if(p==9)pending_action=event->wheel.y>0?991:992;
        else if(p==4)pending_action=inside(dialogue_reply_view,mouse_x,mouse_y)?(event->wheel.y>0?822:823):(event->wheel.y>0?820:821);
        else if(p==2)pending_action=inside(inventory_compare_rect,mouse_x,mouse_y)?(event->wheel.y>0?776:777):inside(inventory_detail_rect,mouse_x,mouse_y)?(event->wheel.y>0?770:771):(event->wheel.y>0?735:736);
        else if(p==1 || p==7){description_scroll+=event->wheel.y>0?-3:3;if(description_scroll<0)description_scroll=0;if(description_scroll>description_max)description_scroll=description_max;}
        return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        if(p==4 && event->key.scancode==SDL_SCANCODE_ESCAPE)return false;
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            if(p==9 && (event->key.scancode==SDL_SCANCODE_PAGEUP || event->key.scancode==SDL_SCANCODE_PAGEDOWN))pending_action=event->key.scancode==SDL_SCANCODE_PAGEUP?991:992;
            if(p==9 && modern_written_is_book() && (event->key.scancode==SDL_SCANCODE_LEFT || event->key.scancode==SDL_SCANCODE_RIGHT))pending_action=event->key.scancode==SDL_SCANCODE_LEFT?991:992;
            if(p==2 && key_is(KB_INVENTORY,event->key.scancode,event->key.mod) && !mainmenu_ui_is_active()
                && intgame_mode_get()!=INTGAME_MODE_QUANTITY)pending_action=730;
            if(p==5 && (key_is(KB_SPELLS,event->key.scancode,event->key.mod) || key_is(KB_SKILLS,event->key.scancode,event->key.mod))) {
                int next=key_is(KB_SPELLS,event->key.scancode,event->key.mod)?1:2;
                selector=selector==next?0:next;
            }
            if((p==1 && key_is(KB_CHARACTER,event->key.scancode,event->key.mod)) || (p==3 && key_is(KB_MAP,event->key.scancode,event->key.mod)))pending_action=p==1?720:740;
            if(event->key.scancode==SDL_SCANCODE_ESCAPE)pending_action=p==9?990:p==1?720:p==2?(intgame_mode_get()==INTGAME_MODE_QUANTITY?764:mainmenu_ui_is_active()?737:730):p==3?740:p==6?908:p==7?929:p==8?969:840;
            if(p==4 && event->key.scancode>=SDL_SCANCODE_1 && event->key.scancode<=SDL_SCANCODE_5)pending_action=800+event->key.scancode-SDL_SCANCODE_1;
            if(p==4 && event->key.scancode==SDL_SCANCODE_PAGEDOWN)pending_action=821;
            if(p==4 && event->key.scancode==SDL_SCANCODE_PAGEUP)pending_action=820;
        }
        return true;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION || event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        bool motion=event->type==SDL_EVENT_MOUSE_MOTION;
        if(p==3 && map_pressed && map_drag_event(event->type,motion?0:event->button.button,NULL))return true;
        for(int i=0;i<panel_region_count;i++)if(inside(panel_regions[i].rect,mouse_x,mouse_y)) {
            PanelRegion* r=&panel_regions[i];int xx=r->x+(mouse_x-r->rect.x)*r->w/r->rect.w,yy=r->y+(mouse_y-r->rect.y)*r->h/r->rect.h;
            if(p==2 && r->w && !inven_ui_drag_item_obj_get()){
                int64_t obj=modern_inventory_at(xx,yy);
                if(obj && !inventory_match(obj))return true;
            }
            if(p==3 && map_drag_event(event->type,motion?0:event->button.button,r))return true;
            int e=motion?TIG_MESSAGE_MOUSE_MOVE:event->type==SDL_EVENT_MOUSE_BUTTON_DOWN?TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
                event->button.button==SDL_BUTTON_RIGHT?TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
            if(motion || event->button.button==SDL_BUTTON_LEFT || event->type==SDL_EVENT_MOUSE_BUTTON_UP)panel_mouse_queue(r->kind,xx,yy,e);
            return true;
        }
        if(!motion) {
            int action=-1;for(int i=button_count-1;i>=0;i--)if(buttons[i].enabled && inside(buttons[i].rect,mouse_x,mouse_y)){action=buttons[i].action;break;}
            if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN){pressed_action=action;click_captured=true;}
            else {if(click_captured && action==pressed_action && event->button.button==SDL_BUTTON_LEFT)pending_action=action;click_captured=false;pressed_action=-1;}
        }
        return true;
    }
    return event->type==SDL_EVENT_TEXT_INPUT;
}
