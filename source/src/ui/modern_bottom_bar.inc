/* ImageGen material only: all icons, counters, liquid and actions are live.
 * The source contains eleven recesses. Omit one central 91 px strip when
 * loading to retain exactly ten slots without stretching the brasswork. */
static SDL_Texture* bottom_bar_texture;
static SDL_Texture* bottom_bar_liquid[2];
#define BOTTOM_BAR_WIDTH 980.0f
#define BOTTOM_BAR_SOURCE_WIDTH 2080.0f
#define BOTTOM_BAR_SOURCE_HEIGHT 436.0f

static void bottom_bar_load(void)
{
    SDL_Surface* source=SDL_LoadPNG("modern-ui/bottom-bar.png");
    if(source) {
        /* Join at source resolution: separate scaled draws can leave a seam
         * when their destination edges fall between physical pixels. */
        SDL_Surface* joined=SDL_CreateSurface(2080,436,SDL_PIXELFORMAT_RGBA32);
        if(joined) {
            SDL_Rect left={0,0,1045,436},right={1136,0,1035,436},destination={1045,0,1035,436};
            SDL_SetSurfaceBlendMode(source,SDL_BLENDMODE_NONE);
            if(SDL_BlitSurface(source,&left,joined,&left) && SDL_BlitSurface(source,&right,joined,&destination))
                bottom_bar_texture=SDL_CreateTextureFromSurface(renderer,joined);
            SDL_DestroySurface(joined);
        }
        SDL_DestroySurface(source);
        if(bottom_bar_texture)SDL_SetTextureScaleMode(bottom_bar_texture,SDL_SCALEMODE_LINEAR);
    }
    /* A continuous texture avoids gaps between independently rounded strips. */
    for(int fatigue=0;fatigue<2;fatigue++) {
        SDL_Surface* gradient=SDL_CreateSurface(128,1,SDL_PIXELFORMAT_RGBA32);
        if(!gradient)continue;
        for(int i=0;i<128;i++) {
            float sheen=powf(sinf(i/127.f*3.14159f),.65f);
            SDL_Color color=fatigue?(SDL_Color){(Uint8)(13+18*sheen),(Uint8)(38+60*sheen),(Uint8)(90+110*sheen),255}:
                (SDL_Color){(Uint8)(65+125*sheen),(Uint8)(10+26*sheen),(Uint8)(17+25*sheen),255};
            SDL_WriteSurfacePixel(gradient,i,0,color.r,color.g,color.b,color.a);
        }
        bottom_bar_liquid[fatigue]=SDL_CreateTextureFromSurface(renderer,gradient);
        SDL_DestroySurface(gradient);
        if(bottom_bar_liquid[fatigue])SDL_SetTextureScaleMode(bottom_bar_liquid[fatigue],SDL_SCALEMODE_LINEAR);
    }
}

static SDL_FRect bottom_bar_rect(float x,float y,float w,float h)
{
    float width=fminf(BOTTOM_BAR_WIDTH,canvas_w-16);
    float scale=width/BOTTOM_BAR_SOURCE_WIDTH;
    return (SDL_FRect){(canvas_w-width)/2+x*scale,
        canvas_h-10-BOTTOM_BAR_SOURCE_HEIGHT*scale+y*scale,w*scale,h*scale};
}

static SDL_FRect bottom_bar_message_rect(void)
{
    SDL_FRect r=bottom_bar_rect(558,174,963,229);
    r.x+=8;r.y+=6;r.w-=16;r.h-=12;
    return r;
}

/* Counters must never truncate, including negative HP and large amounts. */
static void bottom_bar_number(const char* label,SDL_FRect r)
{
    SDL_FRect ink=button_text_bounds(0,label);
    float scale=fminf(1,fminf(r.w/fmaxf(1,ink.w),r.h/fmaxf(1,ink.h)));
    float sx,sy;SDL_GetRenderScale(renderer,&sx,&sy);
    SDL_SetRenderScale(renderer,sx*scale,sy*scale);
    text_draw(0,label,(r.x+(r.w-ink.w*scale)/2)/scale-ink.x,
        (r.y+(r.h-ink.h*scale)/2)/scale-ink.y,GOLD);
    SDL_SetRenderScale(renderer,sx,sy);
}

static void bottom_bar_resource(int value,int maximum,bool fatigue)
{
    SDL_FRect glass=bottom_bar_rect(fatigue?1957:47,90,72,238);
    float amount=fmaxf(0,fminf(1,maximum>0?(float)value/maximum:0));
    float liquid=glass.y+glass.h*(1-amount);
    /* Keep the generated dark glass visible above the liquid. */
    if(amount>0) {
        SDL_FRect filled={glass.x,liquid,glass.w,glass.h*amount};
        if(bottom_bar_liquid[fatigue])SDL_RenderTexture(renderer,bottom_bar_liquid[fatigue],NULL,&filled);
        else fill(filled,fatigue?(SDL_Color){31,98,200,255}:(SDL_Color){190,36,42,255});
    }
    if(amount>0)ellipse(glass.x+glass.w/2,liquid,glass.w/2,1,
        fatigue?(SDL_Color){96,164,226,255}:(SDL_Color){227,100,97,255});
    fill((SDL_FRect){glass.x+4,glass.y+2,1.5f,glass.h-4},(SDL_Color){237,241,230,80});
    SDL_FRect plaque=bottom_bar_rect(fatigue?1951:37,371,92,43);
    char number[64];snprintf(number,sizeof(number),"%d / %d",value,maximum);
    bottom_bar_number(number,plaque);
    SDL_FRect hover=bottom_bar_rect(fatigue?1928:14,12,142,412);
    if(inside(hover,mouse_x,mouse_y)) {
        char hint[256];snprintf(hint,sizeof(hint),"%s : %d / %d",character_text(fatigue?107:106),value,maximum);
        float width=ceilf(text_measure(0,hint))+24;
        SDL_FRect tip={fmaxf(8,fminf(hover.x,canvas_w-width-8)),bottom_bar_rect(0,0,0,0).y-34,width,30};
        hud_panel(tip);button_label(0,hint,tip,true,GOLD);
    }
}

static bool draw_bottom_hover(SDL_FRect content)
{
    if(journal || native_panel() || hud_region(mouse_x,mouse_y))return false;
    const ModernDetails* detail=modern_hover_details();if(!detail)return false;
    bool picture=detail->portrait>=0 || detail->icon>=0;
    if(detail->portrait>=0)draw_portrait(detail->portrait,(SDL_FRect){content.x,content.y+2,48,57});
    else if(detail->icon>=0)draw_art(detail->icon,(SDL_FRect){content.x,content.y+2,48,48},true);
    if(picture){content.x+=58;content.w-=58;}
    char description[40100]="";size_t used=0;
    for(int i=0;i<detail->count;i++) {
        if(!strcmp(detail->lines[i],detail->title))continue;
        bool gauge=false;
        for(int j=0;j<detail->bar_count;j++)if(detail->bar_lines[j]==i)gauge=true;
        if(gauge)continue;
        int written=snprintf(description+used,sizeof(description)-used,"%s%s",
            used?(detail->bar_count?"   |   ":"\n"):"",detail->lines[i]);
        if(written<0 || (size_t)written>=sizeof(description)-used)break;
        used+=(size_t)written;
    }
    /* HUD text uses the same compact font as its counters. Leave permanent
     * room for the gauges and effect icons even with long translated names. */
    float bar_height=detail->bar_count?19:0,extra_height=detail->extra_count?24:0;
    SDL_FRect text={content.x,content.y,content.w,content.h-bar_height-extra_height};
    char body[45000];snprintf(body,sizeof(body),"%s%s%s",detail->title,*description?"\n":"",description);
    const char* translated=game_text(body);
    int rows=text_paragraph(0,18,translated,(SDL_FRect){0,0,text.w,0},0,GOLD,false);
    int visible_rows=(int)(text.h/18);
    /* Long descriptions cycle inside the panel instead of spilling over its
     * frame. Reset the cycle when a different object is inspected. */
    static char previous[45000];static Uint64 since;
    if(strcmp(previous,body)){SDL_strlcpy(previous,body,sizeof(previous));since=SDL_GetTicks();}
    int offset=0;
    if(rows>visible_rows && visible_rows>0)
        offset=((SDL_GetTicks()-since)/3500)%((rows+visible_rows-1)/visible_rows)*visible_rows;
    text_paragraph(0,18,translated,text,offset,GOLD,false);
    float y=content.y+content.h-bar_height-extra_height;
    float gap=12,column=(content.w-gap*(detail->bar_count-1))/fmaxf(1,detail->bar_count);
    for(int j=0;j<detail->bar_count;j++) {
        int i=detail->bar_lines[j];if(i<0 || i>=detail->count)continue;
        int kind=detail->bar_kinds[j];
        SDL_Color color=kind==1?(SDL_Color){105,136,244,255}:kind==2?(SDL_Color){124,194,94,255}:(SDL_Color){216,65,53,255};
        const char* value=game_text(detail->lines[i]);float value_width=ceilf(button_text_bounds(0,value).w)+4;
        float x=content.x+j*(column+gap);
        SDL_FRect bar={x,y+5,fmaxf(4,column-value_width-6),9};
        fill(bar,(SDL_Color){35,30,25,255});outline(bar,MUTED);
        float fraction=fmaxf(0,fminf(1,detail->bar_values[j]/100.f));
        fill((SDL_FRect){bar.x+1,bar.y+1,(bar.w-2)*fraction,bar.h-2},color);
        button_label(0,value,(SDL_FRect){x+bar.w+6,y,column-bar.w-6,18},true,color);
    }
    for(int i=0;i<detail->extra_count;i++)
        draw_art(detail->extra_icons[i],(SDL_FRect){content.x+i*24,y+bar_height,22,22},true);
    return true;
}

static void draw_tactical_bottom_content(int64_t actor,bool ready,SDL_FRect content);

static void draw_bottom_bar(int64_t pc,bool available,bool combat)
{
    if(bottom_bar_texture) {
        SDL_FRect dst=bottom_bar_rect(0,0,BOTTOM_BAR_SOURCE_WIDTH,BOTTOM_BAR_SOURCE_HEIGHT);
        SDL_RenderTexture(renderer,bottom_bar_texture,NULL,&dst);
    } else hud_frame(bottom_bar_rect(0,0,BOTTOM_BAR_SOURCE_WIDTH,BOTTOM_BAR_SOURCE_HEIGHT),true);

    const float slots[]={536,641,746,850,951,1049,1147,1249,1355,1459};
    const float widths[]={94,94,94,93,91,91,91,94,94,93};
    SDL_FRect number_ink=button_text_bounds(0,"0123456789+");
    for(int i=0;i<10;i++) {
        SDL_FRect r=bottom_bar_rect(slots[i],14,widths[i],94);
        Hotkey* hk=!combat || combat_player_controls(pc)?modern_hotkey_get(hotbar_offset()+i):NULL;
        if(hk && hk->type!=HOTKEY_NONE)draw_art_id(hk->art_id,
            (SDL_FRect){r.x+3,r.y+3,r.w-6,r.h-6},true);
        top_bar_control(r,300+hotbar_offset()+i,available);
        char key[8];snprintf(key,sizeof(key),"%s%d",hotbar_offset()?"+":"",(i+1)%10);
        float yy=r.y+r.h-4-number_ink.y-number_ink.h;
        /* A dark outline keeps the key readable over bright item/spell art. */
        text_draw(0,key,r.x+5,yy+1,(SDL_Color){0,0,0,255});
        text_draw(0,key,r.x+4,yy,GOLD);
    }
    top_bar_native_button(bottom_bar_rect(226,36,134,136),combat?1103:7,470,true,available);
    top_bar_native_button(bottom_bar_rect(1774,36,134,136),5,472,true,available);
    top_bar_native_button(bottom_bar_rect(1657,138,134,136),6,473,true,available);
    if(combat) {
        /* The unused recess is decorative; keep both character panels together. */
        SDL_FRect circle=bottom_bar_rect(1791,275,100,102);
        ellipse(circle.x+circle.w/2,circle.y+circle.h/2,circle.w/2,circle.h/2,(SDL_Color){10,9,7,255});
        top_bar_native_button(bottom_bar_rect(174,294,90,96),1105,modern_intgame_icon(0),false,available);
        top_bar_native_button(bottom_bar_rect(280,294,90,96),1104,modern_intgame_icon(1),false,available);
    } else top_bar_native_button(bottom_bar_rect(1774,258,134,136),8,471,true,available);

    draw_art(474,bottom_bar_rect(169,207,56,35),true);
    char gold[32];snprintf(gold,sizeof(gold),"%d",item_gold_get(pc));
    bottom_bar_number(gold,bottom_bar_rect(230,198,185,51));

    SDL_FRect content=bottom_bar_message_rect();
    if(combat)draw_tactical_bottom_content(pc,available,content);
    else if(draw_bottom_hover(content)) {
        /* Hover inspection has priority over transient messages. */
    } else if(hud_message_visible()) {
        int rows=menu_paragraph(message_text,(SDL_FRect){0,0,content.w,0},0,GOLD);
        if(rows*27<=content.h) {
            content.y+=(content.h-rows*27)/2;content.h=rows*27;
            menu_paragraph_aligned(message_text,content,0,GOLD,true);
        } else if(!journal && !native_panel())draw_hud_message();
    } else {
        char status[256];snprintf(status,sizeof(status),"%s %d   ·   %02d:%02d",
            game_text(modern_intgame_text(0)),stat_level_get(pc,STAT_LEVEL),datetime_current_hour(),datetime_current_minute());
        button_label(0,status,content,true,MUTED);
    }
    /* Ammunition stays visible even while a game message occupies the panel. */
    int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON);
    if(weapon!=OBJ_HANDLE_NULL && item_weapon_ammo_type(weapon)!=10000) {
        int type=item_weapon_ammo_type(weapon);
        if(type>=0 && type<TIG_ART_AMMO_TYPE_COUNT)draw_art(250+type,bottom_bar_rect(170,combat?258:285,combat?42:56,combat?28:40),true);
        char ammo[32];snprintf(ammo,sizeof(ammo),"%d",item_ammo_quantity_get(pc,type));
        bottom_bar_number(ammo,bottom_bar_rect(230,combat?258:280,185,combat?28:51));
    }
    draw_experience_gauge(pc,bottom_bar_rect(547,116,1023,25));
    bottom_bar_resource(object_hp_current(pc),object_hp_max(pc),false);
    bottom_bar_resource(critter_fatigue_current(pc),critter_fatigue_max(pc),true);
}
