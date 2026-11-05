/* Creation and the local hero sheet share a draft; only confirmation applies it. */
static struct {
    bool active,dragging,dirty,in_game,choosing;
    int selected;
    int64_t object;
    unsigned char* pixels;
    int width,height;
    float cx,cy,zoom,drag_x,drag_y;
    SDL_FRect crop_rect;
    SDL_Texture *source,*preview_large,*preview_small;
    SDL_Surface *large,*small;
    Uint64 preview_time;
    char filename[256];
    const char *error_fr,*error_en;
} portrait_editor;
static struct {SDL_AtomicInt state,alive;char* path;bool failed;} portrait_picker;

static void portrait_editor_error(const char* fr,const char* en)
{portrait_editor.error_fr=fr;portrait_editor.error_en=en;}
static void portrait_editor_release_image(void)
{
    stbi_image_free(portrait_editor.pixels);portrait_editor.pixels=NULL;
    SDL_DestroyTexture(portrait_editor.source);portrait_editor.source=NULL;
    SDL_DestroyTexture(portrait_editor.preview_large);portrait_editor.preview_large=NULL;
    SDL_DestroyTexture(portrait_editor.preview_small);portrait_editor.preview_small=NULL;
    SDL_DestroySurface(portrait_editor.large);portrait_editor.large=NULL;
    SDL_DestroySurface(portrait_editor.small);portrait_editor.small=NULL;
}
static void portrait_editor_close(void)
{
    portrait_editor_release_image();memset(&portrait_editor,0,sizeof(portrait_editor));
    SDL_CaptureMouse(false);pressed_action=-1;click_captured=false;button_count=0;
}
static void SDLCALL portrait_picker_done(void* unused,const char* const* files,int filter)
{
    (void)unused;(void)filter;
    char* path=files && files[0]?SDL_strdup(files[0]):NULL;
    if(!SDL_GetAtomicInt(&portrait_picker.alive)){SDL_free(path);return;}
    portrait_picker.failed=!files || (files[0] && !path);
    portrait_picker.path=path;
    /* Publish only plain data: SDL may call this on its dialog thread. */
    SDL_SetAtomicInt(&portrait_picker.state,2);
}
static void portrait_editor_browse(void)
{
    if(SDL_GetAtomicInt(&portrait_picker.state))return;
    static const SDL_DialogFileFilter filters[]={
        {"PNG / JPEG / BMP","png;jpg;jpeg;bmp"},
    };
    portrait_editor_error(NULL,NULL);
    SDL_SetAtomicInt(&portrait_picker.alive,1);SDL_SetAtomicInt(&portrait_picker.state,1);
    SDL_ShowOpenFileDialog(portrait_picker_done,NULL,window,filters,1,NULL,false);
}
static float portrait_editor_side(void)
{return fminf((float)portrait_editor.width,(float)portrait_editor.height)/portrait_editor.zoom;}
static void portrait_editor_clamp(void)
{
    float half=portrait_editor_side()/2;
    portrait_editor.cx=fmaxf(half,fminf(portrait_editor.width-half,portrait_editor.cx));
    portrait_editor.cy=fmaxf(half,fminf(portrait_editor.height-half,portrait_editor.cy));
    portrait_editor.dirty=true;
}
static void portrait_editor_zoom(float factor)
{
    if(!portrait_editor.pixels)return;
    portrait_editor.zoom=fmaxf(1,fminf(8,portrait_editor.zoom*factor));portrait_editor_clamp();
}
static bool portrait_editor_preview(void)
{
    float side=portrait_editor_side();
    SDL_Surface* large=portrait_image_crop(portrait_editor.pixels,portrait_editor.width,portrait_editor.height,
        portrait_editor.cx-side/2,portrait_editor.cy-side/2,side,128);
    SDL_Surface* small=portrait_image_crop(portrait_editor.pixels,portrait_editor.width,portrait_editor.height,
        portrait_editor.cx-side/2,portrait_editor.cy-side/2,side,64);
    SDL_Texture* big_texture=large?SDL_CreateTextureFromSurface(renderer,large):NULL;
    SDL_Texture* small_texture=small?SDL_CreateTextureFromSurface(renderer,small):NULL;
    if(!large || !small || !big_texture || !small_texture) {
        SDL_DestroySurface(large);SDL_DestroySurface(small);
        SDL_DestroyTexture(big_texture);SDL_DestroyTexture(small_texture);
        portrait_editor_error("Impossible de préparer le portrait.","Could not prepare the portrait.");return false;
    }
    SDL_DestroySurface(portrait_editor.large);SDL_DestroySurface(portrait_editor.small);
    SDL_DestroyTexture(portrait_editor.preview_large);SDL_DestroyTexture(portrait_editor.preview_small);
    portrait_editor.large=large;portrait_editor.small=small;
    portrait_editor.preview_large=big_texture;portrait_editor.preview_small=small_texture;
    portrait_editor.dirty=false;portrait_editor.preview_time=SDL_GetTicks();return true;
}
static void portrait_editor_poll(void)
{
    if(SDL_GetAtomicInt(&portrait_picker.state)!=2)return;
    char* path=portrait_picker.path;portrait_picker.path=NULL;
    bool failed=portrait_picker.failed;SDL_SetAtomicInt(&portrait_picker.state,0);
    if(!portrait_editor.active){SDL_free(path);return;}
    if(failed)portrait_editor_error("Impossible d’ouvrir le sélecteur de fichier.","Could not open the file picker.");
    else if(!path) {if(!portrait_editor.pixels)portrait_editor_close();}
    else {
        int width,height;unsigned char* pixels=portrait_image_load(path,&width,&height);
        if(!pixels)portrait_editor_error("Image illisible ou trop grande. PNG, JPEG ou BMP ; 8 192 pixels par côté, 24 mégapixels et 64 Mo maximum.",
            "Unreadable or oversized image. PNG, JPEG or BMP; up to 8,192 pixels per side, 24 megapixels and 64 MB.");
        else {
            SDL_Surface* surface=SDL_CreateSurfaceFrom(width,height,SDL_PIXELFORMAT_RGBA32,pixels,width*4);
            SDL_Texture* texture=surface?SDL_CreateTextureFromSurface(renderer,surface):NULL;
            SDL_DestroySurface(surface);
            if(!texture){stbi_image_free(pixels);portrait_editor_error("Impossible d’afficher cette image.","Could not display this image.");}
            else {
                portrait_editor_release_image();portrait_editor.pixels=pixels;portrait_editor.source=texture;
                SDL_SetTextureScaleMode(texture,SDL_SCALEMODE_LINEAR);
                portrait_editor.width=width;portrait_editor.height=height;
                portrait_editor.cx=width/2.f;portrait_editor.cy=height/2.f;portrait_editor.zoom=1;
                const char* name=path;for(const char* p=path;*p;p++)if(*p=='/' || *p=='\\')name=p+1;
                SDL_strlcpy(portrait_editor.filename,name,sizeof(portrait_editor.filename));
                portrait_editor_error(NULL,NULL);portrait_editor_clamp();
            }
        }
    }
    SDL_free(path);
}
static bool portrait_editor_target_valid(void)
{
    if(portrait_editor.in_game)return play_sheet() && !tig_net_is_active()
        && modern_charedit_object()==portrait_editor.object
        && player_get_local_pc_obj()==portrait_editor.object;
    ModernCharacter c;
    return mainmenu_ui_window_type_get()==MM_WINDOW_NEW_CHAR
        && modern_mainmenu_character(&c,0) && c.editable && c.object==portrait_editor.object;
}
static bool portrait_editor_action(int action)
{
    if(action==3000 && !portrait_editor.active) {
        int64_t obj=modern_charedit_object();
        if(!play_sheet() || !obj || obj!=player_get_local_pc_obj() || tig_net_is_active())return true;
        portrait_editor_close();portrait_editor.active=portrait_editor.in_game=portrait_editor.choosing=true;
        portrait_editor.object=obj;portrait_editor.selected=portrait_get(obj);
        SDL_StopTextInput(window);return true;
    }
    if(action==512 && !portrait_editor.active) {
        ModernCharacter c;
        if(mainmenu_ui_window_type_get()!=MM_WINDOW_NEW_CHAR || !modern_mainmenu_character(&c,0) || !c.editable)return true;
        portrait_editor_close();portrait_editor.active=true;portrait_editor.object=c.object;
        name_focused=false;SDL_StopTextInput(window);portrait_editor_browse();return true;
    }
    if(!portrait_editor.active)return false;
    if(!portrait_editor_target_valid()){portrait_editor_close();return true;}
    if(SDL_GetAtomicInt(&portrait_picker.state))return true;
    if(portrait_editor.choosing) {
        if(action==3001)portrait_find_prev(portrait_editor.object,&portrait_editor.selected);
        else if(action==3002)portrait_find_next(portrait_editor.object,&portrait_editor.selected);
        else if(action==515){
            obj_field_int32_set(portrait_editor.object,OBJ_F_CRITTER_PORTRAIT,portrait_editor.selected);
            portrait_number=-1;portrait_editor_close();
        } else if(action==514)portrait_editor_close();
        else if(action==513){portrait_editor.choosing=false;portrait_editor_browse();}
        return true;
    }
    if(action==513)portrait_editor_browse();
    else if(action==514)portrait_editor_close();
    else if(action==516)portrait_editor_zoom(1/1.15f);
    else if(action==517)portrait_editor_zoom(1.15f);
    else if(action==518 && portrait_editor.pixels) {
        portrait_editor.zoom=1;portrait_editor.cx=portrait_editor.width/2.f;
        portrait_editor.cy=portrait_editor.height/2.f;portrait_editor_clamp();
    } else if(action==515 && portrait_editor.pixels) {
        if(portrait_editor.dirty && !portrait_editor_preview())return true;
        int id;PortraitImportResult result=portrait_import(portrait_editor.small,portrait_editor.large,&id);
        if(result==PORTRAIT_IMPORT_OK) {
            obj_field_int32_set(portrait_editor.object,OBJ_F_CRITTER_PORTRAIT,id);portrait_number=-1;portrait_editor_close();
        } else if(result==PORTRAIT_IMPORT_FULL)
            portrait_editor_error("Le catalogue contient déjà 999 portraits personnalisés.","The catalogue already contains 999 custom portraits.");
        else if(result==PORTRAIT_IMPORT_CATALOGUE)
            portrait_editor_error("Le catalogue a changé ou ses numéros ne se suivent pas. Vérifiez userport.mes et redémarrez le jeu.",
                "The catalogue changed or has gaps in its numbering. Check userport.mes and restart the game.");
        else portrait_editor_error("Import impossible. Vérifiez que le dossier data/portrait est accessible en écriture.",
            "Import failed. Check that the data/portrait folder is writable.");
    }
    return true;
}
static void draw_portrait_editor(void)
{
    if(portrait_editor.choosing) {
        float w=fminf(720,canvas_w-40),h=660,x=(canvas_w-w)/2,y=(canvas_h-h)/2;
        frame((SDL_FRect){x,y,w,h});
        button_label(3,tr("Changer de portrait","Change portrait"),(SDL_FRect){x+24,y+24,w-48,52},true,GOLD);
        draw_portrait(portrait_editor.selected,(SDL_FRect){canvas_w/2-128,y+100,256,256});
        int previous=portrait_editor.selected,next=portrait_editor.selected;
        bool has_previous=portrait_find_prev(portrait_editor.object,&previous);
        bool has_next=portrait_find_next(portrait_editor.object,&next);
        menu_button((SDL_FRect){x+24,y+198,70,54},3001,"<",has_previous);
        menu_button((SDL_FRect){x+w-94,y+198,70,54},3002,">",has_next);
        menu_button((SDL_FRect){x+60,y+390,w-120,52},513,tr("Importer une image…","Import an image…"),true);
        menu_paragraph(tr("Le portrait sera conservé dans votre prochaine sauvegarde.",
            "The portrait will be stored in your next save."),(SDL_FRect){x+32,y+466,w-64,100},0,MUTED);
        menu_button((SDL_FRect){x+24,y+h-76,(w-72)/2,52},514,tr("Annuler","Cancel"),true);
        menu_button((SDL_FRect){x+w/2+12,y+h-76,(w-72)/2,52},515,tr("Utiliser ce portrait","Use this portrait"),true);
        return;
    }
    float w=fminf(1120,canvas_w-40),h=800,x=(canvas_w-w)/2,y=(canvas_h-h)/2;
    frame((SDL_FRect){x,y,w,h});
    centered(3,tr("Importer un portrait","Import portrait"),canvas_w/2,y+22,GOLD);
    rule(x+30,y+75,w-60,MUTED);
    bool waiting=SDL_GetAtomicInt(&portrait_picker.state)!=0,loaded=portrait_editor.pixels!=NULL;
    menu_button((SDL_FRect){x+32,y+94,240,42},513,tr("Choisir une image…","Choose image…"),!waiting);
    button_label(1,portrait_editor.filename,(SDL_FRect){x+290,y+96,w-322,38},false,MUTED);
    if(loaded && portrait_editor.dirty && SDL_GetTicks()-portrait_editor.preview_time>120)portrait_editor_preview();
    float edge=440;
    portrait_editor.crop_rect=(SDL_FRect){x+32,y+160,edge,edge};
    fill(portrait_editor.crop_rect,(SDL_Color){30,22,16,255});
    if(loaded) {
        float side=portrait_editor_side();
        SDL_FRect src={portrait_editor.cx-side/2,portrait_editor.cy-side/2,side,side};
        SDL_RenderTexture(renderer,portrait_editor.source,&src,&portrait_editor.crop_rect);
    }
    outline(portrait_editor.crop_rect,GOLD);
    float right=x+510,rw=w-542;
    text_draw(2,tr("Résultat en jeu","In-game result"),right,y+162,GOLD);
    menu_paragraph(tr("Les deux formats sont convertis en 256 couleurs, compatibles avec le jeu classique.",
        "Both sizes are converted to 256 colors, compatible with the classic game."),(SDL_FRect){right,y+205,rw,122},0,MUTED);
    SDL_FRect big={right,y+340,160,160},small={right+190,y+340,80,80};
    fill(big,(SDL_Color){30,22,16,255});fill(small,(SDL_Color){30,22,16,255});
    if(portrait_editor.preview_large)SDL_RenderTexture(renderer,portrait_editor.preview_large,NULL,&big);
    if(portrait_editor.preview_small)SDL_RenderTexture(renderer,portrait_editor.preview_small,NULL,&small);
    outline(big,GOLD);outline(small,GOLD);
    text_draw(0,"128 × 128",right,y+513,MUTED);text_draw(0,"64 × 64",right+190,y+433,MUTED);
    menu_paragraph(tr("Glissez l’image pour cadrer le visage. Utilisez la molette ou les boutons pour zoomer.",
        "Drag the image to frame the face. Use the wheel or buttons to zoom."),(SDL_FRect){right,y+550,rw,122},0,MUTED);
    menu_button((SDL_FRect){x+32,y+620,48,40},516,"-",loaded && !waiting && portrait_editor.zoom>1);
    char zoom[32];snprintf(zoom,sizeof(zoom),"%.0f %%",portrait_editor.zoom*100);
    centered(1,loaded?zoom:"",x+144,y+629,GOLD);
    menu_button((SDL_FRect){x+208,y+620,48,40},517,"+",loaded && !waiting && portrait_editor.zoom<8);
    menu_button((SDL_FRect){x+272,y+620,200,40},518,tr("Recentrer","Reset crop"),loaded && !waiting);
    if(portrait_editor.error_fr)menu_paragraph(tr(portrait_editor.error_fr,portrait_editor.error_en),(SDL_FRect){x+32,y+674,w-64,58},0,UI_RED);
    else if(waiting)text_draw(1,tr("Choisissez une image dans la fenêtre ouverte.","Choose an image in the open dialog."),x+32,y+685,MUTED);
    menu_button((SDL_FRect){x+32,y+742,240,42},514,tr("Annuler","Cancel"),!waiting);
    menu_button((SDL_FRect){x+w-272,y+742,240,42},515,tr("Importer et utiliser","Import and use"),loaded && !waiting);
}
static bool portrait_editor_event(SDL_Event* event,int ww,int wh)
{
    if(!portrait_editor.active)return false;
    if(SDL_GetAtomicInt(&portrait_picker.state))return event->type==SDL_EVENT_MOUSE_MOTION
        || event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP
        || event->type==SDL_EVENT_MOUSE_WHEEL || event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP || event->type==SDL_EVENT_TEXT_INPUT;
    if(event->type==SDL_EVENT_WINDOW_FOCUS_LOST) {portrait_editor.dragging=false;SDL_CaptureMouse(false);}
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
            if(event->key.scancode==SDL_SCANCODE_ESCAPE)pending_action=514;
            if(event->key.scancode==SDL_SCANCODE_RETURN)pending_action=515;
        }
        return true;
    }
    if(event->type==SDL_EVENT_TEXT_INPUT)return true;
    if(event->type==SDL_EVENT_MOUSE_WHEEL) {portrait_editor_zoom(powf(1.12f,event->wheel.y));return true;}
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button==SDL_BUTTON_LEFT && portrait_editor.pixels) {
        float x=event->button.x*canvas_w/ww,y=event->button.y*canvas_h/wh;
        if(inside(portrait_editor.crop_rect,x,y)) {
            portrait_editor.dragging=true;portrait_editor.drag_x=x;portrait_editor.drag_y=y;
            SDL_CaptureMouse(true);return true;
        }
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_UP && event->button.button==SDL_BUTTON_LEFT && portrait_editor.dragging) {
        portrait_editor.dragging=false;SDL_CaptureMouse(false);return true;
    }
    if(event->type==SDL_EVENT_MOUSE_MOTION && portrait_editor.dragging) {
        float x=event->motion.x*canvas_w/ww,y=event->motion.y*canvas_h/wh;
        float scale=portrait_editor_side()/portrait_editor.crop_rect.w;
        portrait_editor.cx-=(x-portrait_editor.drag_x)*scale;portrait_editor.cy-=(y-portrait_editor.drag_y)*scale;
        portrait_editor.drag_x=x;portrait_editor.drag_y=y;portrait_editor_clamp();return true;
    }
    return false;
}
