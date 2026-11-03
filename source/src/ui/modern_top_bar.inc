/* The generated material is a skin only. Controls, counts and the clock use
 * native game data/art. Coordinates below refer to the tightly cropped skin. */
static SDL_Texture* top_bar_texture;
#define TOP_BAR_WIDTH 930.0f
#define TOP_BAR_SOURCE_WIDTH 1370.0f
#define TOP_BAR_SOURCE_HEIGHT 92.0f

static SDL_FRect top_bar_rect(float x,float y,float w,float h)
{
    float scale=TOP_BAR_WIDTH/TOP_BAR_SOURCE_WIDTH;
    return (SDL_FRect){(canvas_w-TOP_BAR_WIDTH)/2+x*scale,y*scale,w*scale,h*scale};
}

static void top_bar_control(SDL_FRect r,int action,bool available)
{
    if(!available)fill(r,(SDL_Color){0,0,0,95});
    else if(inside(r,mouse_x,mouse_y)) {
        if(pressed_action==action)fill(r,(SDL_Color){0,0,0,80});
        outline((SDL_FRect){r.x+2,r.y+2,r.w-4,r.h-4},GOLD);
    }
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){r,action,available};
}

/* Use the original normal, pressed and hovered art frames, especially for
 * circular buttons: highlighting must not add a square around their rim. */
static void top_bar_native_button(SDL_FRect r,int action,int number,bool round,bool available)
{
    int state=available && inside(r,mouse_x,mouse_y)?(pressed_action==action?1:2):0;
    tig_art_id_t id;
    if(tig_art_interface_id_create(number,0,0,0,&id)==TIG_OK) {
        /* Native highlighted art can contain fewer frames than regular buttons. */
        for(int i=0;i<state;i++)id=tig_art_id_frame_inc(id);
        ArtTexture* art=art_get(id);
        if(!art && tig_art_interface_id_create(number,0,0,0,&id)==TIG_OK)art=art_get(id);
        if(art) {
            if(!available)SDL_SetTextureColorMod(art->texture,140,140,140);
            draw_art_id(id,r,round);
            SDL_SetTextureColorMod(art->texture,255,255,255);
        }
    }
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){r,action,available};
}

/* Match the original scrolling clock, including its eight moon phases. */
static void top_bar_clock(SDL_FRect r,DateTime now)
{
    int64_t seconds=datetime_seconds_since_reference_date(&now);
    const int phase_limits[]={0,4,9,13,14,18,23,27};
    int phase=((seconds+43200)/84600)%28,moon=0;
    while(moon<7 && phase>phase_limits[moon])moon++;
    ArtTexture* strips[2];tig_art_id_t id;
    if(tig_art_interface_id_create(207,0,0,0,&id)!=TIG_OK)return;
    strips[0]=art_get(id);
    if(tig_art_interface_id_create(208+moon,0,0,0,&id)!=TIG_OK)return;
    strips[1]=art_get(id);
    if(!strips[0] || !strips[1])return;
    int total=strips[0]->width+strips[1]->width;
    int offset=(total+(seconds+73800)%86400*total/86400-64)%total;
    fill(r,(SDL_Color){0,0,0,255});
    int remaining=128,drawn=0,cycle=0;
    for(int n=0;n<3 && remaining>0;n++,cycle=1-cycle) {
        ArtTexture* art=strips[cycle];
        if(offset>=art->width){offset-=art->width;continue;}
        int width=SDL_min(art->width-offset,remaining);
        SDL_FRect src={offset,0,width,30};
        SDL_FRect dst={r.x+drawn*r.w/128,r.y,width*r.w/128,r.h};
        SDL_RenderTexture(renderer,art->texture,&src,&dst);
        remaining-=width;drawn+=width;offset=0;
    }
    if(tig_art_interface_id_create(216,0,0,0,&id)==TIG_OK) {
        ArtTexture* pointer=art_get(id);
        if(pointer) {
            /* Limit the original marker to the clock aperture. */
            float w=fminf(pointer->width,68),h=fminf(pointer->height,29);
            SDL_FRect src={0,0,w,h},dst={r.x+60*r.w/128,r.y+r.h/30,w*r.w/128,h*r.h/30};
            SDL_RenderTexture(renderer,pointer->texture,&src,&dst);
        }
    }
}

static void draw_top_bar(bool available)
{
    SDL_FRect r=top_bar_rect(0,0,TOP_BAR_SOURCE_WIDTH,TOP_BAR_SOURCE_HEIGHT);
    if(top_bar_texture)SDL_RenderTexture(renderer,top_bar_texture,NULL,&r);
    else hud_frame(r,false);
    /* Replace the entire generated pictograms, including their borders. */
    const int slots[]={0,2,3,1},actions[]={1,0,3,2};
    const float positions[]={134,206,277,349};
    for(int i=0;i<4;i++) {
        r=top_bar_rect(positions[i],6,71,76);
        fill(r,(SDL_Color){12,13,9,255});
        top_bar_native_button(r,actions[i],modern_intgame_icon(slots[i]),false,available);
    }
    r=top_bar_rect(10,16,58,58);
    top_bar_native_button(r,4,137,true,available);
    r=top_bar_rect(76,36,43,25);
    fill(r,(SDL_Color){0,0,0,255});
    char value[24];snprintf(value,sizeof(value),"%02d",stat_level_get(player_get_local_pc_obj(),STAT_FATE_POINTS));
    button_label(1,value,r,true,GOLD);
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){r,4,available};
    r=top_bar_rect(998,15,60,62);
    top_bar_native_button(r,9,137,true,available);
    SDL_FRect clock_rect=top_bar_rect(1076,17,250,57);
    DateTime now=sub_45A7C0();top_bar_clock(clock_rect,now);
    if(inside(clock_rect,mouse_x,mouse_y)) {
        char raw[256],label[1024];datetime_format_datetime(&now,raw,sizeof(raw));
        SDL_strlcpy(label,game_text(raw),sizeof(label));
        float width=fminf(540,text_measure(0,label)+28);
        SDL_FRect tip={fminf(canvas_w-width-8,clock_rect.x),r.y+r.h+12,width,30};
        hud_panel(tip);button_label(0,label,tip,true,GOLD);
    }
}

static SDL_FRect top_bar_spell_rect(int slot)
{
    return top_bar_rect(541+slot*70,16,60,61);
}
