/* The module script owns the order and the installed slide resources. */
typedef struct CreditPage {
    SDL_Texture* image;
    SDL_Surface* pixels;
    SDL_FRect crop;
    char voice[TIG_MAX_PATH];
} CreditPage;

/* Compare a coarse grid to group cards painted on the same background.
 * Cards with independent artwork keep their complete image. */
static bool credit_same_background(const SDL_Surface* a,const SDL_Surface* b)
{
    if(!a || !b || a->w!=b->w || a->h!=b->h) return false;
    int similar=0;
    for(int row=0;row<12;row++) {
        int y=(2*row+1)*a->h/24;
        for(int col=0;col<16;col++) {
            int x=(2*col+1)*a->w/32;
            const Uint8* p=(const Uint8*)a->pixels+y*a->pitch+4*x;
            const Uint8* q=(const Uint8*)b->pixels+y*b->pitch+4*x;
            if(abs(p[0]-q[0])<40 && abs(p[1]-q[1])<40 && abs(p[2]-q[2])<40) similar++;
        }
    }
    return similar>=170;
}

static SDL_FRect credit_text_crop(const SDL_Surface* page,const SDL_Surface* paper)
{
    SDL_FRect full={0,0,(float)page->w,(float)page->h};
    int left=page->w,right=-1,top=page->h,bottom=-1;
    int lines=0,band_height=0,max_band=0;
    for(int y=0;y<page->h;y++) {
        const Uint8* p=(const Uint8*)page->pixels+y*page->pitch;
        const Uint8* q=(const Uint8*)paper->pixels+y*paper->pitch;
        int last=-8,previous=-8;
        bool row=false;
        for(int x=0;x<page->w;x++,p+=4,q+=4) {
            /* The first test also finds headings repeated on multiple cards.
             * The second finds varying bright lettering of any hue. */
            bool ink=(p[0]>=220 && p[1]>=47 && p[1]<105 && abs(p[2]-p[1])<=4 && p[0]-p[1]>135)
                || p[0]-q[0]>80 || p[1]-q[1]>80 || p[2]-q[2]>80;
            if(!ink) continue;
            if(x-previous<=7) {
                left=SDL_min(left,previous);right=SDL_max(right,x);
                row=true;
            }
            previous=last;
            last=x;
        }
        if(row) {
            if(!band_height) lines++;
            band_height++;
            max_band=SDL_max(max_band,band_height);
            top=SDL_min(top,y);bottom=y;
        } else band_height=0;
    }
    /* A blank card, a lone emblem or a large illustration supplies too little
     * evidence for trimming. Keep the original aspect and every pixel. */
    if(lines<3 || max_band>page->h/7 || right-left<page->w/18 || bottom-top<page->h/10) return full;
    int margin=SDL_max(8,page->h/44);
    left=SDL_max(0,left-margin/2);right=SDL_min(page->w,right+1+margin/2);
    top=SDL_max(0,top-margin);bottom=SDL_min(page->h,bottom+1+margin);
    return (SDL_FRect){(float)left,(float)top,(float)(right-left),(float)(bottom-top)};
}

static void credit_prepare_crops(CreditPage* pages,int count)
{
    int reference=-1,best=1;
    for(int i=0;i<count;i++) {
        int matches=0;
        for(int j=0;j<count;j++) {
            if(credit_same_background(pages[i].pixels,pages[j].pixels)) matches++;
        }
        if(matches>best) {best=matches;reference=i;}
    }
    if(reference<0) return;
    SDL_Surface* exemplar=pages[reference].pixels;
    SDL_Surface* paper=SDL_DuplicateSurface(exemplar);
    if(!paper) return;
    /* The registered cards share a dark printed background. Its dimmest
     * channel values suppress both the stained glass and the blue globe. */
    for(int i=0;i<count;i++) {
        SDL_Surface* image=pages[i].pixels;
        if(!credit_same_background(exemplar,image)) continue;
        for(int y=0;y<image->h;y++) {
            Uint8* dst=(Uint8*)paper->pixels+y*paper->pitch;
            const Uint8* src=(const Uint8*)image->pixels+y*image->pitch;
            for(int x=0;x<image->w*4;x++) dst[x]=SDL_min(dst[x],src[x]);
        }
    }
    for(int i=0;i<count;i++) {
        if(credit_same_background(exemplar,pages[i].pixels))
            pages[i].crop=credit_text_crop(pages[i].pixels,paper);
    }
    SDL_DestroySurface(paper);
}

enum CreditAction { CREDIT_NONE, CREDIT_PREVIOUS, CREDIT_PAUSE, CREDIT_NEXT, CREDIT_RETURN };
static struct {
    CreditPage* pages;
    int index, count;
    bool paused;
    enum CreditAction pressed, action;
    Uint64 page_started;
} album;

static SDL_FRect credit_button(enum CreditAction action)
{
    float width=fminf(202,(canvas_w-40)/4);
    return (SDL_FRect){canvas_w/2+(action-CREDIT_PREVIOUS-2)*width+3,
        canvas_h-69,width-6,52};
}

static bool credit_available(enum CreditAction action)
{
    return (action!=CREDIT_PREVIOUS || album.index>0)
        && (action!=CREDIT_NEXT || album.index+1<album.count);
}

static bool credit_event(SDL_Event* event)
{
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            switch(event->key.scancode) {
            case SDL_SCANCODE_KP_4:
            case SDL_SCANCODE_LEFT: album.action=CREDIT_PREVIOUS; break;
            case SDL_SCANCODE_KP_6:
            case SDL_SCANCODE_RIGHT: album.action=CREDIT_NEXT; break;
            case SDL_SCANCODE_SPACE: album.action=CREDIT_PAUSE; break;
            case SDL_SCANCODE_ESCAPE: album.action=CREDIT_RETURN; break;
            default: break;
            }
        }
        return true;
    }
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST) album.pressed=CREDIT_NONE;
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        int w,h;
        SDL_GetWindowSize(window,&w,&h);
        enum CreditAction hit=CREDIT_NONE;
        if(w>0 && h>0 && event->button.button==SDL_BUTTON_LEFT) {
            for(int i=CREDIT_PREVIOUS;i<=CREDIT_RETURN;i++)
                if(credit_available(i) && inside(credit_button(i),event->button.x*canvas_w/w,event->button.y*canvas_h/h)) hit=i;
        }
        if(event->button.down) album.pressed=hit;
        else {
            if(hit==album.pressed) album.action=hit;
            album.pressed=CREDIT_NONE;
        }
        return true;
    }
    return event->type==SDL_EVENT_MOUSE_MOTION || event->type==SDL_EVENT_MOUSE_WHEEL;
}

static void draw_credits(SDL_Renderer* target)
{
    renderer=target;
    update_geometry();
    int old_w,old_h,w,h;
    SDL_RendererLogicalPresentation old_mode;
    SDL_GetRenderLogicalPresentation(renderer,&old_w,&old_h,&old_mode);
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){0,0,0,255});
    float decor_scale=fmaxf(canvas_w/1672,canvas_h/941);
    SDL_FRect decor={(canvas_w-1672*decor_scale)/2,(canvas_h-941*decor_scale)/2,
        1672*decor_scale,941*decor_scale};
    SDL_SetTextureColorMod(single_player_texture,135,135,135);
    SDL_RenderTexture(renderer,single_player_texture,NULL,&decor);
    SDL_SetTextureColorMod(single_player_texture,255,255,255);
    SDL_FRect title={canvas_w/2-145,12,290,61};
    menu_plaque(title,true,false);
    centered(3,menu_text(sub_5496C0(MM_WINDOW_MAINMENU)->num+2),canvas_w/2,title.y+8,GOLD);

    CreditPage* current=&album.pages[album.index];
    float panel_h=canvas_h-196;
    float panel_w=fminf(canvas_w-32,fmaxf(canvas_h*.69f,
        fminf(canvas_h*1.12f,panel_h*current->crop.w/current->crop.h+30)));
    SDL_FRect panel={(canvas_w-panel_w)/2,81,panel_w,panel_h};
    SDL_BlendMode old_blend;
    SDL_GetRenderDrawBlendMode(renderer,&old_blend);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    hud_panel(panel);
    SDL_SetRenderDrawBlendMode(renderer,old_blend);
    float scale=fminf((panel.w-36)/current->crop.w,(panel.h-36)/current->crop.h);
    SDL_FRect page={canvas_w/2-current->crop.w*scale/2,
        panel.y+(panel.h-current->crop.h*scale)/2,current->crop.w*scale,current->crop.h*scale};
    Uint64 age=SDL_GetTicks()-album.page_started;
    Uint8 light=age>=320?255:(Uint8)(255*age/320);
    SDL_SetTextureColorMod(current->image,light,light,light);
    SDL_RenderTexture(renderer,current->image,&current->crop,&page);

    char number[32];
    snprintf(number,sizeof(number),"%d / %d",album.index+1,album.count);
    SDL_FRect counter={canvas_w/2-50,canvas_h-114,100,34};
    menu_plaque(counter,true,false);
    main_menu_label(counter,number,GOLD,false,false);
    SDL_GetWindowSize(window,&w,&h);
    float x,y;
    SDL_GetMouseState(&x,&y);
    if(w>0 && h>0) {mouse_x=x*canvas_w/w;mouse_y=y*canvas_h/h;}
    const char* labels[]={NULL,tr("Précédente","Previous"),
        album.paused?tr("Reprendre","Resume"):tr("Pause","Pause"),
        tr("Suivante","Next"),tr("Retour","Back")};
    for(int i=CREDIT_PREVIOUS;i<=CREDIT_RETURN;i++) {
        SDL_FRect rect=credit_button(i);
        bool available=credit_available(i), down=album.pressed==i;
        menu_plaque(rect,available,down);
        main_menu_label(rect,labels[i],menu_control_color(rect,false,available),down,false);
    }
    if(SDL_GetMouseFocus()==window) draw_art(0,(SDL_FRect){mouse_x,mouse_y,28,35},true);
    SDL_SetRenderLogicalPresentation(renderer,old_w,old_h,old_mode);
}

bool modern_ui_credits(const int* slides,int count,
    bool (*assets)(int,char*,size_t,char*,size_t))
{
    if(!enabled || !single_player_texture || !main_menu_texture || count<=0) return false;
    CreditPage* pages=calloc(count,sizeof(*pages));
    if(!pages) return false;
    int loaded=0;
    for(;loaded<count;loaded++) {
        char bmp[TIG_MAX_PATH],path[TIG_MAX_PATH];
        if(!assets(slides[loaded],bmp,sizeof(bmp),pages[loaded].voice,sizeof(pages[loaded].voice))
            || !tig_file_extract(bmp,path)) break;
        SDL_Surface* surface=SDL_LoadBMP(path);
        if(!surface) break;
        pages[loaded].image=SDL_CreateTextureFromSurface(renderer,surface);
        pages[loaded].crop=(SDL_FRect){0,0,(float)surface->w,(float)surface->h};
        if(pages[loaded].image) pages[loaded].pixels=SDL_ConvertSurface(surface,SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(surface);
        if(!pages[loaded].image) break;
        SDL_SetTextureScaleMode(pages[loaded].image,SDL_SCALEMODE_LINEAR);
    }
    if(loaded==count) credit_prepare_crops(pages,count);
    for(int i=0;i<loaded;i++) SDL_DestroySurface(pages[i].pixels);
    if(loaded==count) {
        album.pages=pages;
        album.count=count;
        tig_video_set_slide_overlay(draw_credits);
        gsound_stop_all(25);
        tig_sound_handle_t music=gsound_play_music_indefinitely("sound\\music\\DwarvenMusic.mp3",25);
        bool quit=false;
        while(album.index<count && !quit) {
            album.page_started=SDL_GetTicks();
            Uint64 elapsed=0,previous=album.page_started;
            tig_sound_handle_t voice=gsound_play_voice(pages[album.index].voice,0);
            for(;;) {
                tig_ping();
                TigMessage msg;
                while(tig_message_dequeue(&msg)==TIG_OK) {
                    if(msg.type==TIG_MESSAGE_QUIT) {
                        tig_message_enqueue(&msg);
                        quit=true;
                        break;
                    }
                }
                Uint64 now=SDL_GetTicks();
                if(!album.paused) elapsed+=now-previous;
                previous=now;
                enum CreditAction action=album.action;
                album.action=CREDIT_NONE;
                if(quit || action==CREDIT_RETURN) {quit=true;break;}
                if(action==CREDIT_PAUSE) album.paused=!album.paused;
                if((action==CREDIT_NEXT || action==CREDIT_PREVIOUS) && credit_available(action)) {
                    album.paused=true;
                    album.index+=action==CREDIT_NEXT?1:-1;
                    break;
                }
                if(!album.paused && elapsed>=10000
                    && (voice==TIG_SOUND_HANDLE_INVALID || !tig_sound_is_playing(voice))) {
                    album.index++;
                    break;
                }
                tig_video_flip();
                SDL_Delay(10);
            }
            if(voice!=TIG_SOUND_HANDLE_INVALID) tig_sound_destroy(voice);
        }
        tig_video_set_slide_overlay(NULL);
        if(music!=TIG_SOUND_HANDLE_INVALID) tig_sound_destroy(music);
        memset(&album,0,sizeof(album));
        if(SDL_GetMouseFocus()==window) {
            SDL_FRect credits=main_menu_plaque(main_menu_bounds(),2);
            int w,h;
            SDL_GetWindowSize(window,&w,&h);
            SDL_WarpMouseInWindow(window,(credits.x+credits.w/2)*w/canvas_w,
                (credits.y+credits.h/2)*h/canvas_h);
        }
    }
    for(int i=0;i<loaded;i++) SDL_DestroyTexture(pages[i].image);
    free(pages);
    return loaded==count;
}
