/* Button text has its own font scale: body text and native UI stay independent.
 * Fit by wrapping/truncating, never by shrinking the requested character size. */
static char button_overflow_hint[1024];

typedef struct ButtonTextLayout {
    char lines[2][1024];
    SDL_FRect ink[2];
    int count;
    float step, height;
    bool shortened;
} ButtonTextLayout;

static SDL_FRect button_text_bounds(int font_id,const char* text)
{
    NativeFont* font=&fonts[font_id];
    float pen=0,left=0,right=0,top=font->size,bottom=0;
    while(*text) {
        int c=codepoint(&text);stbtt_packedchar* g=&font->glyphs[c-GLYPH_FIRST];
        if(g->x1>g->x0 && g->y1>g->y0) {
            left=fminf(left,pen+g->xoff/font->density);
            right=fmaxf(right,pen+g->xoff2/font->density);
            top=fminf(top,font->size+g->yoff/font->density);
            bottom=fmaxf(bottom,font->size+g->yoff2/font->density);
        }
        pen+=g->xadvance/font->density;
    }
    return (SDL_FRect){left,top,fmaxf(right,pen)-left,fmaxf(0,bottom-top)};
}

static void button_text_pop(char* text)
{
    size_t n=strlen(text);
    if(n) {do {n--;}while(n && ((unsigned char)text[n]&0xc0)==0x80);text[n]=0;}
}

static bool button_text_fit(int font_id,char* text,float width)
{
    if(button_text_bounds(font_id,text).w<=width)return false;
    char prefix[1024];SDL_strlcpy(prefix,text,sizeof(prefix));
    do {
        button_text_pop(prefix);
        SDL_strlcpy(text,prefix,1024);SDL_strlcat(text,"...",1024);
    } while(*prefix && button_text_bounds(font_id,text).w>width);
    if(button_text_bounds(font_id,text).w>width)text[0]=0;
    return true;
}

static ButtonTextLayout button_text_layout(int font_id,const char* text,float width,float height)
{
    ButtonTextLayout layout={0};layout.count=1;
    /* Copy complete UTF-8 codepoints, including at the buffer boundary. */
    const char* end=text;
    while(*end) {
        const char* next=end;codepoint(&next);
        if(next-text>=(int)sizeof(layout.lines[0])-4)break;
        end=next;
    }
    memcpy(layout.lines[0],text,end-text);layout.lines[0][end-text]=0;
    layout.shortened=*end!=0;
    if(layout.shortened)SDL_strlcat(layout.lines[0],"...",sizeof(layout.lines[0]));
    SDL_FRect full=button_text_bounds(font_id,layout.lines[0]);
    float best=full.w;
    int split=-1;
    if(full.w>width)for(int i=1;layout.lines[0][i];i++) {
        if(layout.lines[0][i]!=' ' || !layout.lines[0][i+1])continue;
        layout.lines[0][i]=0;
        SDL_FRect a=button_text_bounds(font_id,layout.lines[0]);
        SDL_FRect b=button_text_bounds(font_id,layout.lines[0]+i+1);
        layout.lines[0][i]=' ';
        float widest=fmaxf(a.w,b.w);
        if(a.h+b.h+2<=height && widest<best) {best=widest;split=i;}
    }
    if(split>0) {
        SDL_strlcpy(layout.lines[1],layout.lines[0]+split+1,sizeof(layout.lines[1]));
        layout.lines[0][split]=0;layout.count=2;
    }
    for(int i=0;i<layout.count;i++) {
        if(button_text_fit(font_id,layout.lines[i],width))layout.shortened=true;
        layout.ink[i]=button_text_bounds(font_id,layout.lines[i]);
    }
    layout.step=layout.ink[0].h+2;
    layout.height=layout.ink[0].h+(layout.count==2?2+layout.ink[1].h:0);
    return layout;
}

static void button_label(int font_id,const char* text,SDL_FRect rect,bool center,SDL_Color color)
{
    if(!text || !*text || rect.w<=0 || rect.h<=0)return;
    ButtonTextLayout layout=button_text_layout(font_id,text,rect.w,rect.h);
    bool clipped=SDL_RenderClipEnabled(renderer);SDL_Rect old_clip,clip;
    SDL_GetRenderClipRect(renderer,&old_clip);
    clip=(SDL_Rect){(int)ceilf(rect.x),(int)ceilf(rect.y),
        (int)floorf(rect.x+rect.w)-(int)ceilf(rect.x),(int)floorf(rect.y+rect.h)-(int)ceilf(rect.y)};
    if(clipped)SDL_GetRectIntersection(&clip,&old_clip,&clip);
    SDL_SetRenderClipRect(renderer,&clip);
    float y=rect.y+(rect.h-layout.height)/2;
    for(int i=0;i<layout.count;i++) {
        SDL_FRect ink=layout.ink[i];
        text_draw(font_id,layout.lines[i],rect.x+(center?(rect.w-ink.w)/2:0)-ink.x,
            y-ink.y,color);
        y+=layout.step;
    }
    SDL_SetRenderClipRect(renderer,clipped?&old_clip:NULL);
    if(layout.shortened && inside(rect,mouse_x,mouse_y))
        SDL_strlcpy(button_overflow_hint,text,sizeof(button_overflow_hint));
}
