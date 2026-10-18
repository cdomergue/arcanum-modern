/* Native play screens. Included after the shared rendering helpers. */
typedef struct PanelRegion { SDL_FRect rect; int x,y,w,h,kind; } PanelRegion;
static PanelRegion panel_regions[32];
static int panel_region_count, selector, selector_college, panel_previous;
static bool sheet_summary;
static int dialogue_scroll, dialogue_max;
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
static bool map_pressed,map_dragged;
static PanelRegion map_drag_region;
static float map_start_x,map_start_y,map_last_x,map_last_y,map_pan_x,map_pan_y;
static int map_press_x,map_press_y;

static bool play_sheet(void) { return !mainmenu_ui_is_active() && charedit_is_created(); }
static int native_panel(void)
{
    if(!enabled || !in_game || menu_suspended)return 0;
    if(mainmenu_ui_is_active())return mainmenu_ui_window_type_get()==MM_WINDOW_SHOP && inven_ui_is_created()?2:0;
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
static void draw_sheet_summary(void)
{
    int64_t obj=modern_charedit_object();
    float w=fminf(1360,canvas_w-40),x=(canvas_w-w)/2,y=(canvas_h-800)/2;
    frame((SDL_FRect){x,y,w,800});
    centered(3,actor_name(obj),canvas_w/2,y+26,GOLD);
    draw_portrait(portrait_get(obj),(SDL_FRect){x+40,y+100,180,220});
    text_draw(2,game_text(race_name(stat_level_get(obj,STAT_RACE))),x+40,y+344,GOLD);
    text_draw(1,game_text(gender_name(stat_level_get(obj,STAT_GENDER))),x+40,y+384,MUTED);
    for(int i=0;i<24;i++) {
        int stat=i;float xx=x+270+(i/12)*(w-320)/2,yy=y+105+(i%12)*45;
        char value[48];snprintf(value,sizeof(value),"%d",stat_level_get(obj,stat));
        text_draw(1,game_text(stat_name(stat)),xx,yy,GOLD);
        text_draw(1,value,xx+(w-320)/2-70,yy,MUTED);
        rule(xx,yy+33,(w-320)/2-45,(SDL_Color){81,59,35,255});
    }
    menu_button((SDL_FRect){x+30,y+726,280,54},724,menu_text(739),true);
    menu_button((SDL_FRect){x+w-310,y+726,280,54},720,menu_text(260),true);
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
        if(!obj || obj==inven_ui_drag_item_obj_get())continue;
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
    if(object!=inventory_detail_object || kind!=inventory_detail_kind)inventory_detail_scroll=0;
    inventory_detail_object=object;inventory_detail_kind=kind;
    modern_inventory_details(pc,object,kind,&inventory_details);
}
static void inventory_detail_draw(void)
{
    SDL_FRect r=inventory_detail_rect;
    fill(r,(SDL_Color){19,16,12,245});outline(r,(SDL_Color){130,100,57,255});
    if(!inventory_details.count)return;
    if(inventory_details.icon>=0)draw_art(inventory_details.icon,(SDL_FRect){r.x+18,r.y+18,76,76},true);
    menu_paragraph(game_text(inventory_details.lines[0]),(SDL_FRect){r.x+110,r.y+20,r.w-132,81},0,GOLD);
    rule(r.x+18,r.y+110,r.w-36,MUTED);
    char body[40000]={0};
    for(int i=1;i<inventory_details.count;i++) {
        SDL_strlcat(body,game_text(inventory_details.lines[i]),sizeof(body));
        SDL_strlcat(body,"\n",sizeof(body));
    }
    SDL_FRect content={r.x+20,r.y+126,r.w-58,r.h-150};
    int rows=menu_paragraph(body,(SDL_FRect){0,0,content.w,0},0,GOLD);
    inventory_detail_max=rows-(int)(content.h/27);if(inventory_detail_max<0)inventory_detail_max=0;
    if(inventory_detail_scroll>inventory_detail_max)inventory_detail_scroll=inventory_detail_max;
    menu_paragraph(body,content,inventory_detail_scroll,GOLD);
    if(inventory_detail_max) {
        menu_button((SDL_FRect){r.x+r.w-36,r.y+125,28,38},770,"^",inventory_detail_scroll>0);
        menu_button((SDL_FRect){r.x+r.w-36,r.y+r.h-55,28,38},771,"v",inventory_detail_scroll<inventory_detail_max);
    }
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
    inventory_detail_rect=(SDL_FRect){x+830,y+120,w-855,610};
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
    inventory_detail_draw();
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
static void draw_map(void)
{
    ModernMap map;if(!modern_map_state(&map))return;
    panel_backdrop();float w=fminf(1440,canvas_w-40),x=(canvas_w-w)/2,y=(canvas_h-820)/2;
    frame((SDL_FRect){x,y,w,820});
    centered(3,game_text(modern_map_text(map.mode==1?502:501)),canvas_w/2,y+22,GOLD);
    menu_button((SDL_FRect){x+30,y+78,260,48},741,game_text(modern_map_text(501)),true);
    menu_button((SDL_FRect){x+310,y+78,260,48},742,game_text(modern_map_text(502)),true);
    if(SDL_GetTicks()-map_texture_time>50 || map_texture_mode!=map.mode || !map_texture) {
        float ww,hh;SDL_DestroyTexture(map_texture);map_texture=texture_from_buffer(map.buffer,&ww,&hh);
        map_texture_time=SDL_GetTicks();map_texture_mode=map.mode;
    }
    SDL_FRect available={x+30,y+150,w-60,550};
    float scale=fminf(available.w/map.width,available.h/map.height);
    SDL_FRect dest={available.x+(available.w-map.width*scale)/2,available.y+(available.h-map.height*scale)/2,map.width*scale,map.height*scale};
    SDL_FRect src={map.x,map.y-41,map.width,map.height};
    fill(available,(SDL_Color){12,10,8,255});
    if(map_texture)SDL_RenderTexture(renderer,map_texture,&src,&dest);outline(dest,GOLD);
    panel_region(dest,map.x,map.y,map.width,map.height,3);
    menu_button((SDL_FRect){x+30,y+741,230,50},740,menu_text(4003),true);
    text_in_button((SDL_FRect){x+w-350,y+741,320,50},743,game_text(modern_map_text(512)),false,map.mode!=1);
    if(*message_text && SDL_GetTicks()-message_time<12000)menu_paragraph(message_text,(SDL_FRect){x+290,y+736,w-670,54},0,GOLD);
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
        if(modern_fate_active(i))text_draw(1,"+",r.x+r.w-25,r.y+5,GOLD);
    }
    menu_button((SDL_FRect){x+w-250,y+578,225,46},969,menu_text(260),true);
}
static void draw_play_panels(void)
{
    panel_region_count=0;
    switch(native_panel()) {
    case 1:panel_backdrop();if(sheet_summary)draw_sheet_summary();else draw_character_sheet();break;
    case 2:draw_inventory();break;
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
    if(action==950 || action==951) {
        if(!p && (intgame_mode_get()==INTGAME_MODE_MAIN || intgame_mode_get()==INTGAME_MODE_SKILL)) {
            if(action==950 && intgame_mode_get()==INTGAME_MODE_SKILL && sub_579F90()==1)skill_ui_cancel();
            else sub_579FA0(combat_input_actor(),action==950?1:0);
        }
        return true;
    }
    if(p==8 && action>=969 && action<982){modern_fate_choose(action-970);return true;}
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
        if(intgame_mode_get()==INTGAME_MODE_MAIN && !p){selector=action==6?1:2;return true;}
    }
    if(p==1 && action>=600 && action<700)return creation_menu_action(action);
    if(action==720 && p==1){charedit_close();return true;}
    if(action==724 && p==1){sheet_summary=!sheet_summary;return true;}
    if(action==737 && p==2 && mainmenu_ui_is_active()){modern_mainmenu_activate(1);return true;}
    if(action==730 && p==2 && mainmenu_ui_is_active()){modern_mainmenu_activate(0);return true;}
    if(action>=730 && action<=736 && p==2) {
        if(action>=735)modern_inventory_scroll(action==735?-1:1);
        else modern_inventory_action(action-730);
        return true;
    }
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
    int p=native_panel();
    if(p!=panel_previous) {
        button_count=0;panel_region_count=0;pressed_action=-1;click_captured=false;
        if(p==1){sheet_summary=true;sheet_category=sheet_page=sheet_selected=description_scroll=0;}
        if(p!=5)selector=0;
        map_pressed=map_dragged=false;map_pan_x=map_pan_y=0;SDL_CaptureMouse(false);
        if(p==2){memset(&inventory_details,0,sizeof(inventory_details));inventory_detail_object=0;inventory_detail_scroll=0;}
        panel_previous=p;
    }
    if(p==3 && ((int)map_pan_x || (int)map_pan_y)) {
        int dx=(int)map_pan_x,dy=(int)map_pan_y;modern_map_pan(dx,dy);
        map_pan_x-=dx;map_pan_y-=dy;map_texture_time=0;
    }
    for(int i=0;i<panel_event_count;i++) {
        if(panel_events[i].kind==p) {
            if(p==2)modern_inventory_mouse(panel_events[i].x,panel_events[i].y,panel_events[i].event);
            if(p==3)modern_map_mouse(panel_events[i].x,panel_events[i].y,panel_events[i].event);
        }
    }
    panel_event_count=0;
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
        map_pressed=map_dragged=false;map_pan_x=map_pan_y=0;SDL_CaptureMouse(false);return false;
    }
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);if(ww<=0 || wh<=0)return false;
    if(event->type==SDL_EVENT_MOUSE_MOTION){mouse_x=event->motion.x*canvas_w/ww;mouse_y=event->motion.y*canvas_h/wh;}
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP){mouse_x=event->button.x*canvas_w/ww;mouse_y=event->button.y*canvas_h/wh;}
    if(event->type==SDL_EVENT_MOUSE_WHEEL) {
        if(p==4)pending_action=inside(dialogue_reply_view,mouse_x,mouse_y)?(event->wheel.y>0?822:823):(event->wheel.y>0?820:821);
        else if(p==2)pending_action=inside(inventory_detail_rect,mouse_x,mouse_y)?(event->wheel.y>0?770:771):(event->wheel.y>0?735:736);
        else if(p==1 || p==7){description_scroll+=event->wheel.y>0?-3:3;if(description_scroll<0)description_scroll=0;if(description_scroll>description_max)description_scroll=description_max;}
        return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        if(p==4 && event->key.scancode==SDL_SCANCODE_ESCAPE)return false;
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            if(event->key.scancode==SDL_SCANCODE_ESCAPE)pending_action=p==1?720:p==2?(intgame_mode_get()==INTGAME_MODE_QUANTITY?764:mainmenu_ui_is_active()?737:730):p==3?740:p==6?908:p==7?929:p==8?969:840;
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
            if(p==3 && map_drag_event(event->type,motion?0:event->button.button,r))return true;
            int e=motion?TIG_MESSAGE_MOUSE_MOVE:event->type==SDL_EVENT_MOUSE_BUTTON_DOWN?TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN:
                event->button.button==SDL_BUTTON_RIGHT?TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP:TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
            if(p==2 && event->type==SDL_EVENT_MOUSE_BUTTON_UP
                && event->button.button==SDL_BUTTON_LEFT && event->button.clicks==2)
                e=MODERN_INVENTORY_DOUBLE_CLICK;
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
