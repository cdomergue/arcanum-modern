/* Arcanum Modern: native-resolution presentation, independent of world pixels.
 * Original artwork is read from the user's DAT files; gameplay remains upstream.
 */
#include "ui/modern_ui.h"
#include "ui/modern_graphics.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "../../third_party/stb/stb_truetype.h"

#include "game/critter.h"
#include "game/combat.h"
#include "game/location.h"
#include "game/dialog.h"
#include "game/hrp.h"
#include "ui/charedit_ui.h"
#include "ui/inven_ui.h"
#include "ui/follower_ui.h"
#include "ui/wmap_ui.h"
#include "ui/tb_ui.h"
#include "ui/spell_ui.h"
#include "ui/skill_ui.h"
#include "game/gamelib.h"
#include "game/object.h"
#include "game/obj_pool.h"
#include "game/item.h"
#include "game/stat.h"
#include "game/level.h"
#include "game/resistance.h"
#include "game/player.h"
#include "game/quest.h"
#include "game/portrait.h"
#include "game/skill.h"
#include "game/spell.h"
#include "game/tech.h"
#include "game/timeevent.h"
#include "ui/hotkey_ui.h"
#include "ui/intgame.h"
#include "ui/logbook_ui.h"
#include "ui/mainmenu_ui.h"
#include "ui/slide_ui.h"
#include "ui/written_ui.h"
#include "ui/schematic_ui.h"
#include <tig/tig.h>

#define GLYPH_FIRST 32
#define GLYPH_COUNT 352
#define FONT_COUNT 4
#define MAX_BUTTONS 256

typedef struct NativeFont {
    SDL_Texture* texture;
    stbtt_packedchar glyphs[GLYPH_COUNT];
    float density;
    float size;
} NativeFont;

typedef struct ArtTexture {
    tig_art_id_t id;
    SDL_Texture* texture;
    int width, height;
} ArtTexture;

typedef struct Button {
    SDL_FRect rect;
    int action;
    bool enabled;
} Button;

typedef struct BookLine {
    char text[512];
    int style;
    bool heading,entry_start;
    int entry;
} BookLine;

static const SDL_Color GOLD = {210, 178, 105, 255};
static const SDL_Color INK = {47, 35, 25, 255};
static const SDL_Color MUTED = {163, 145, 111, 255};
static const char* chapters[] = {"Rumeurs et notes", "Quêtes", "Réputations",
    "Bénédictions / malédictions", "Combats", "Origines", "Trousseau"};
static const char* chapter_labels[] = {"Rumeurs et notes", "Quêtes", "Réputations",
    "Bénéd. / maléd.", "Combats", "Origines", "Trousseau"};
static const int chapter_art[] = {265, 267, 271, 266, 270, 268, 269};
static SDL_Renderer* renderer;
static SDL_Window* window;
static NativeFont fonts[FONT_COUNT];
static unsigned char *book_font_data, *heading_font_data;
static size_t book_font_size, heading_font_size;
static ArtTexture arts[512];
static int art_count;
static SDL_Texture *wood_texture, *paper_texture;
static SDL_Texture* menu_background;
static SDL_Texture* skill_icons_texture;
static SDL_Texture* control_icons_texture;
static float menu_background_width, menu_background_height;
static Button buttons[MAX_BUTTONS];
static int button_count;
static BookLine* lines;
static size_t line_count, line_capacity;
static bool enabled, in_game, hud, journal, searching, dirty;
static bool click_captured;
static bool menu_suspended;
static int options_tab=-1;
static mes_file_handle_t option_boolean_file=MES_FILE_HANDLE_INVALID;
static int menu_previous_type = -1;
static bool confirming_menu;
static int confirmation_choice;
static char confirmation_text[512];
static int pressed_action = -1, pending_action = -1;
static int chapter, quest_filter, spread, lines_per_page, spread_count;
static int entry_count, matched_count;
static int output_width, output_height;
static float canvas_w = 1600, canvas_h = 900, density = 1;
static float mouse_x, mouse_y, text_width;
static int body_size = 20;
static char search[128];
static char message_text[1024];
static Uint64 message_time;
static SDL_FRect panel, book;

static void render_overlay(SDL_Renderer* target);
static int native_panel(void);
static bool panels_event(SDL_Event* event);
static bool panels_action(int action);
static bool tactical_active(void);
static bool maintained_region(float x,float y);
static bool party_region(float x,float y);
static int hit(float x,float y);
static void draw_tactical_spells(void);
static void panels_tick(void);
static void draw_play_panels(void);
static bool play_sheet(void);
static const char* game_text(const char* source);
static const char* tr(const char* fr,const char* en);
static bool handle_event(SDL_Event* event);
static bool handle_ui_event(SDL_Event* event);
static void rebuild_book(void);
static void capture_reviews(void);
static void verify_text_layout(void);
static bool capturing, captured;

static int codepoint(const char** text)
{
    const unsigned char* p = (const unsigned char*)*text;
    int c = *p++;
    if (c >= 0xC2 && c < 0xE0 && (*p & 0xC0) == 0x80) {
        c = ((c & 31) << 6) | (*p++ & 63);
    } else if (c >= 0xE0 && c < 0xF0 && p[0] && p[1]
        && (p[0] & 0xC0) == 0x80 && (p[1] & 0xC0) == 0x80) {
        c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63);
        p += 2;
    }
    *text = (const char*)p;
    if (c == 0x2018 || c == 0x2019) c = '\'';
    if (c == 0x201C || c == 0x201D) c = '"';
    if (c == 0x2013 || c == 0x2014) c = '-';
    if (c == 0x2026) c = '.';
    if (c < GLYPH_FIRST || c >= GLYPH_FIRST + GLYPH_COUNT) c = '?';
    return c;
}

/* Game strings are Windows-1252; labels and search are UTF-8. */
static void from_game(const char* src, char* dst, size_t capacity)
{
    static const unsigned short cp1252[32] = {
        0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
        0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
        0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
        0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178};
    size_t n = 0;
    if (!src || !capacity) return;
    while (*src && n + 4 < capacity) {
        unsigned c = (unsigned char)*src++;
        if (c >= 128 && c < 160) c = cp1252[c - 128];
        if (c < 128) dst[n++] = (char)c;
        else if (c < 2048) {
            dst[n++] = (char)(192 | (c >> 6));
            dst[n++] = (char)(128 | (c & 63));
        } else {
            dst[n++] = (char)(224 | (c >> 12));
            dst[n++] = (char)(128 | ((c >> 6) & 63));
            dst[n++] = (char)(128 | (c & 63));
        }
    }
    dst[n] = 0;
}

static SDL_Texture* rgba_texture(int w, int h, Uint32* pixels)
{
    SDL_Texture* t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STATIC, w, h);
    if (t) {
        SDL_UpdateTexture(t, NULL, pixels, w * 4);
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_SCALEMODE_LINEAR);
    }
    return t;
}

/* Startup precedes UI initialization: rasterize this localized label without
 * touching the HUD's renderer, fonts or geometry. */
SDL_Texture* modern_ui_loading_label(SDL_Renderer* target,const char* source,float size)
{
    size_t length;
    unsigned char* data=SDL_LoadFile("modern-ui/Book.ttf",&length);
    if(!data)return NULL;
    stbtt_fontinfo font;
    if(!stbtt_InitFont(&font,data,stbtt_GetFontOffsetForIndex(data,0))){SDL_free(data);return NULL;}
    char text[2048];from_game(source,text,sizeof(text));
    float scale=stbtt_ScaleForPixelHeight(&font,size),width=0;
    int ascent,descent,gap;
    stbtt_GetFontVMetrics(&font,&ascent,&descent,&gap);
    const char* p=text;
    while(*p){int advance,bearing;stbtt_GetCodepointHMetrics(&font,codepoint(&p),&advance,&bearing);width+=advance*scale;}
    SDL_Surface* surface=SDL_CreateSurface((int)ceilf(width)+8,(int)ceilf((ascent-descent)*scale)+8,SDL_PIXELFORMAT_RGBA32);
    if(!surface){SDL_free(data);return NULL;}
    SDL_FillSurfaceRect(surface,NULL,0);
    float pen=4;int baseline=(int)ceilf(ascent*scale)+4;
    p=text;
    while(*p) {
        int c=codepoint(&p),w,h,xoff,yoff,advance,bearing;
        unsigned char* bitmap=stbtt_GetCodepointBitmap(&font,scale,scale,c,&w,&h,&xoff,&yoff);
        if(bitmap)for(int y=0;y<h;y++)for(int x=0;x<w;x++) {
            int dx=(int)roundf(pen)+xoff+x,dy=baseline+yoff+y;
            if(dx<0 || dx>=surface->w || dy<0 || dy>=surface->h)continue;
            unsigned char* pixel=(unsigned char*)surface->pixels+dy*surface->pitch+dx*4;
            unsigned a=bitmap[y*w+x];
            pixel[0]=pixel[1]=pixel[2]=255;pixel[3]=(unsigned char)(a+pixel[3]*(255-a)/255);
        }
        stbtt_FreeBitmap(bitmap,NULL);
        stbtt_GetCodepointHMetrics(&font,c,&advance,&bearing);pen+=advance*scale;
    }
    SDL_Texture* texture=SDL_CreateTextureFromSurface(target,surface);
    SDL_DestroySurface(surface);SDL_free(data);
    return texture;
}

static bool make_font(NativeFont* font, unsigned char* data, float size)
{
    const int side = 2048;
    unsigned char* bitmap = calloc(side * side, 1);
    Uint32* pixels = malloc(side * side * sizeof(*pixels));
    if (!bitmap || !pixels) { free(bitmap); free(pixels); return false; }
    stbtt_pack_context pack;
    stbtt_PackBegin(&pack, bitmap, side, side, 0, 1, NULL);
    bool ok = stbtt_PackFontRange(&pack, data, 0, size * density,
        GLYPH_FIRST, GLYPH_COUNT, font->glyphs);
    stbtt_PackEnd(&pack);
    if (ok) {
        for (int i = 0; i < side * side; i++) pixels[i] = ((Uint32)bitmap[i] << 24) | 0xFFFFFF;
        SDL_DestroyTexture(font->texture);
        font->texture = rgba_texture(side, side, pixels);
        font->density = density;
        font->size = size;
        ok = font->texture != NULL;
    }
    free(bitmap); free(pixels);
    return ok;
}

static float text_measure(int font_id, const char* text)
{
    NativeFont* font = &fonts[font_id];
    float width = 0;
    while (*text) {
        int c = codepoint(&text);
        width += font->glyphs[c - GLYPH_FIRST].xadvance / font->density;
    }
    return width;
}

static void text_draw(int font_id, const char* text, float x, float y, SDL_Color color)
{
    NativeFont* font = &fonts[font_id];
    if (!font->texture) return;
    SDL_SetTextureColorMod(font->texture, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font->texture, color.a);
    float pen = x;
    while (*text) {
        int c = codepoint(&text);
        stbtt_packedchar* g = &font->glyphs[c - GLYPH_FIRST];
        SDL_FRect src = {g->x0, g->y0, g->x1 - g->x0, g->y1 - g->y0};
        SDL_FRect dst = {pen + g->xoff / font->density,
            y + font->size + g->yoff / font->density,
            (g->xoff2 - g->xoff) / font->density,
            (g->yoff2 - g->yoff) / font->density};
        SDL_RenderTexture(renderer, font->texture, &src, &dst);
        pen += g->xadvance / font->density;
    }
}

/* Compact labels retain their full localized form in a hover tooltip. */
static void text_compact(const char* text,float x,float y,float width,bool center,SDL_Color color)
{
    char label[1024];SDL_strlcpy(label,text,sizeof(label));
    bool shortened=false;
    while(*label && text_measure(0,label)>width-10) {
        size_t n=strlen(label);do {n--;}while(n>0 && ((unsigned char)label[n]&0xc0)==0x80);
        label[n]=0;shortened=true;
    }
    if(shortened)SDL_strlcat(label,"…",sizeof(label));
    text_draw(0,label,x+(center?(width-text_measure(0,label))/2:0),y,color);
}

static void centered(int font_id, const char* text, float center, float y, SDL_Color color)
{
    text_draw(font_id, text, center - text_measure(font_id, text) / 2, y, color);
}

static void fill(SDL_FRect r, SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderFillRect(renderer, &r);
}

static void outline(SDL_FRect r, SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderRect(renderer, &r);
}

static void rule(float x, float y, float w, SDL_Color c)
{
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_RenderLine(renderer, x, y, x + w, y);
}

static bool inside(SDL_FRect r, float x, float y)
{
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static void material(SDL_Texture* texture, SDL_FRect rect)
{
    SDL_RenderTexture(renderer, texture, NULL, &rect);
}

static void frame(SDL_FRect rect)
{
    fill((SDL_FRect){rect.x + 6, rect.y + 9, rect.w, rect.h}, (SDL_Color){0,0,0,135});
    material(wood_texture, rect);
    outline(rect, (SDL_Color){14,9,6,255});
    outline((SDL_FRect){rect.x+2,rect.y+2,rect.w-4,rect.h-4}, (SDL_Color){126,94,48,255});
    outline((SDL_FRect){rect.x+5,rect.y+5,rect.w-10,rect.h-10}, (SDL_Color){48,30,19,255});
    for (int i=0;i<4;i++) {
        float x=rect.x+(i%2 ? rect.w-12 : 12), y=rect.y+(i/2 ? rect.h-12 : 12);
        fill((SDL_FRect){x-2,y-2,5,5}, (SDL_Color){32,20,10,255});
        fill((SDL_FRect){x-1,y-2,3,3}, GOLD);
    }
}

static ArtTexture* art_get(tig_art_id_t id)
{
    for (int i=0;i<art_count;i++) if (arts[i].id==id) return &arts[i];
    if (art_count >= (int)SDL_arraysize(arts)) return NULL;
    TigArtFrameData fd;
    if (tig_art_frame_data(id,&fd)!=TIG_OK || fd.width<=0 || fd.height<=0) return NULL;
    TigVideoBufferCreateInfo ci = {0};
    ci.width=fd.width; ci.height=fd.height;
    ci.background_color=tig_color_make(255,0,255);
    TigVideoBuffer* vb;
    if (tig_video_buffer_create(&ci,&vb)!=TIG_OK) return NULL;
    TigRect rect={0,0,fd.width,fd.height};
    TigArtBlitInfo bi={0};
    bi.art_id=id; bi.src_rect=&rect; bi.dst_rect=&rect; bi.dst_video_buffer=vb;
    if (tig_art_blit(&bi)!=TIG_OK) {tig_video_buffer_destroy(vb);return NULL;}
    TigVideoBufferData bd;
    tig_video_buffer_lock(vb);
    tig_video_buffer_data(vb,&bd);
    Uint32* pixels=malloc(fd.width*fd.height*4);
    if (!pixels) {tig_video_buffer_unlock(vb);tig_video_buffer_destroy(vb);return NULL;}
    for(int y=0;y<fd.height;y++) for(int x=0;x<fd.width;x++) {
        Uint32 p=((Uint32*)((unsigned char*)bd.pixels+y*bd.pitch))[x];
        pixels[y*fd.width+x]=(p&0xFFFFFF)==(ci.background_color&0xFFFFFF) ? 0 : (p|0xFF000000);
    }
    ArtTexture* art=&arts[art_count++];
    art->id=id;art->width=fd.width;art->height=fd.height;
    art->texture=rgba_texture(fd.width,fd.height,pixels);
    free(pixels);tig_video_buffer_unlock(vb);tig_video_buffer_destroy(vb);
    return art;
}

static void draw_art_id(tig_art_id_t id, SDL_FRect rect, bool fit)
{
    if(skill_icons_texture && tig_art_type(id)==TIG_ART_TYPE_INTERFACE) {
        const int icons[]={279,280,282,278};
        /* Sample the supplied atlas directly: retain its alpha and silver frames. */
        const SDL_FRect cells[]={{62,63,539,523},{655,63,539,523},{62,656,539,526},{655,656,539,526}};
        for(int i=0;i<4;i++)if(tig_art_num_get(id)==icons[i]) {
            SDL_FRect src=cells[i];
            if(fit){float scale=fminf(rect.w/src.w,rect.h/src.h);rect.x+=(rect.w-src.w*scale)/2;rect.y+=(rect.h-src.h*scale)/2;rect.w=src.w*scale;rect.h=src.h*scale;}
            SDL_RenderTexture(renderer,skill_icons_texture,&src,&rect);return;
        }
    }
    ArtTexture* art=art_get(id);
    if (!art || !art->texture) return;
    if (fit) {
        float s=fminf(rect.w/art->width,rect.h/art->height);
        rect.x+=(rect.w-art->width*s)/2;rect.y+=(rect.h-art->height*s)/2;
        rect.w=art->width*s;rect.h=art->height*s;
    }
    SDL_RenderTexture(renderer,art->texture,NULL,&rect);
}

static void draw_art(int number, SDL_FRect rect, bool fit)
{
    tig_art_id_t id;
    if(tig_art_interface_id_create(number,0,0,0,&id)==TIG_OK) draw_art_id(id,rect,fit);
}

/* Generated brass atlas: source bounds exclude transparent cell padding. */
static bool control_symbol(SDL_FRect rect,const char* label,bool available,bool highlighted)
{
    static const char* labels[]={"<","^","v",">","+","-","<<",">>"};
    static const SDL_FRect cells[]={
        {93,98,236,310},{527,105,305,268},{948,141,320,245},{1447,97,233,311},
        {88,507,294,292},{512,599,315,112},{924,524,324,260},{1411,524,303,256}
    };
    if(!control_icons_texture || !label)return false;
    int index=-1;
    for(int i=0;i<8;i++)if(strcmp(label,labels[i])==0){index=i;break;}
    if(index<0)return false;
    SDL_FRect src=cells[index];
    float side=fminf(28,fminf(rect.w,rect.h)*.62f);
    float scale=side/fmaxf(src.w,src.h);
    SDL_FRect dst={rect.x+(rect.w-src.w*scale)/2,rect.y+(rect.h-src.h*scale)/2,src.w*scale,src.h*scale};
    Uint8 brightness=available?(highlighted?255:215):145;
    SDL_SetTextureColorMod(control_icons_texture,brightness,brightness,brightness);
    SDL_SetTextureAlphaMod(control_icons_texture,available?255:140);
    SDL_RenderTexture(renderer,control_icons_texture,&src,&dst);
    return true;
}

static void button(SDL_FRect rect, int action, const char* label, int icon, bool active, bool available)
{
    bool hover=available && inside(rect,mouse_x,mouse_y);
    fill(rect,(SDL_Color){(active||hover)?69:35,(active||hover)?48:25,(active||hover)?27:18,245});
    outline(rect,active?GOLD:(SDL_Color){112,83,45,255});
    rule(rect.x+1,rect.y+1,rect.w-2,(SDL_Color){145,111,62,100});
    if(icon>=0) draw_art(icon,(SDL_FRect){rect.x+9,rect.y+6,rect.h-12,rect.h-12},true);
    if(label && *label && !(icon<0 && control_symbol(rect,label,available,active||hover))) {
        if(icon>=0) text_compact(label,rect.x+rect.h+3,rect.y+(rect.h-14)/2-3,rect.w-rect.h-6,false,available?GOLD:MUTED);
        else centered(0,label,rect.x+rect.w/2,rect.y+(rect.h-14)/2-3,available?GOLD:MUTED);
    }
    if(!available) fill(rect,(SDL_Color){0,0,0,85});
    if(button_count<MAX_BUTTONS) buttons[button_count++]=(Button){rect,action,available};
}

#include "modern_notebook.inc"

static void add_line(const char* text,int style,bool heading)
{
    if(line_count==line_capacity) {
        size_t capacity=line_capacity?line_capacity*2:256;
        BookLine* grown=realloc(lines,capacity*sizeof(*lines));
        if(!grown) return;
        lines=grown;line_capacity=capacity;
    }
    SDL_strlcpy(lines[line_count].text,text,sizeof(lines[line_count].text));
    lines[line_count].style=style;lines[line_count].heading=heading;
    lines[line_count].entry=current_entry;lines[line_count].entry_start=false;line_count++;
}

static void wrap_text(const char* input,int style)
{
    const char *p=input;
    bool first=true;
    while(*p) {
        while(*p==' ' || *p=='\r' || *p=='\t') p++;
        if(*p=='\n') {p++;first=false;continue;}
        if(!*p) break;
        const char *start=p,*scan=p,*last_space=NULL,*end=p;
        float width=0;
        int f=(first && chapter<4)?0:1;
        while(*scan && *scan!='\n' && *scan!='\r') {
            const char* prev=scan;
            int c=codepoint(&scan);
            float advance=fonts[f].glyphs[c-GLYPH_FIRST].xadvance/fonts[f].density;
            if(width+advance>text_width || scan-start>=500) {
                end=last_space?last_space:prev;
                if(end==start) end=scan;
                break;
            }
            if(*prev==' ' || *prev=='\t') last_space=prev;
            width+=advance;end=scan;
        }
        if(end==start) {p++;continue;}
        char row[512];size_t length=(size_t)(end-start);
        memcpy(row,start,length);row[length]=0;
        add_line(row,style,f==0);
        p=end;
        while(*p==' ' || *p=='\t') p++;
        if(*p=='\n' || *p=='\r') {p++;first=false;}
    }
    add_line("",style,false);
}

static void fold(const char* src,char* dst,size_t size)
{
    size_t n=0;
    while(*src && n+1<size) {
        int c=codepoint(&src);
        if(c>='A' && c<='Z') c+=32;
        if(c>=0xC0 && c<=0xDF) c+=32;
        if(c>=0xE0 && c<=0xE5) c='a';
        if(c>=0xE8 && c<=0xEB) c='e';
        if(c>=0xEC && c<=0xEF) c='i';
        if(c>=0xF2 && c<=0xF6) c='o';
        if(c>=0xF9 && c<=0xFC) c='u';
        if(c==0xE7) c='c';
        dst[n++]=c<128?(char)c:'?';
    }
    dst[n]=0;
}

static bool quest_filter_matches(int filter,int state)
{
    switch(filter) {
    case 1:return state==QUEST_STATE_ACCEPTED || state==QUEST_STATE_ACHIEVED;
    case 2:return state==QUEST_STATE_COMPLETED;
    case 3:return state==QUEST_STATE_BOTCHED || state==QUEST_STATE_OTHER_COMPLETED;
    case 4:return state==QUEST_STATE_MENTIONED;
    default:return state!=QUEST_STATE_UNKNOWN;
    }
}

static void rebuild_book(void)
{
    char raw[32768],utf8[65536],haystack[65536],needle[256];
    notebook_sync();
    line_count=0;matched_count=0;
    entry_count=modern_logbook_select(chapter);
    fold(search,needle,sizeof(needle));
    for(int i=0;i<entry_count;i++) {
        int style=modern_logbook_entry(i,raw,sizeof(raw));
        current_entry=modern_logbook_id(i);
        if(journal_favorites && journal_mark_index(chapter,current_entry)<0)continue;
        if(chapter==1 && !quest_filter_matches(quest_filter,quest_state_get(player_get_local_pc_obj(),current_entry)))continue;
        from_game(raw,utf8,sizeof(utf8));
        fold(utf8,haystack,sizeof(haystack));
        if(*needle && !strstr(haystack,needle)) continue;
        if(!*utf8) continue;
        matched_count++;
        size_t first=line_count;wrap_text(utf8,style);if(line_count>first)lines[first].entry_start=true;
    }
    current_entry=-1;
    if(line_count==0) add_line(*search?"Aucune entrée ne correspond à la recherche.":"Aucune entrée dans cette rubrique.",0,false);
    /* Keep a date heading with its following line, including on narrow screens. */
    BookLine* unpaged=lines;
    size_t unpaged_count=line_count;
    lines=NULL;line_count=0;line_capacity=0;
    for(size_t i=0;i<unpaged_count;i++) {
        if(unpaged[i].heading && i+1<unpaged_count
            && line_count % lines_per_page == (size_t)(lines_per_page-1)) add_line("",0,false);
        if(!unpaged[i].text[0] && line_count % lines_per_page == 0) continue;
        add_line(unpaged[i].text,unpaged[i].style,unpaged[i].heading);
        lines[line_count-1].entry=unpaged[i].entry;lines[line_count-1].entry_start=unpaged[i].entry_start;
    }
    free(unpaged);
    spread_count=(int)((line_count+2*lines_per_page-1)/(2*lines_per_page));
    if(spread_count<1) spread_count=1;
    if(journal_jump>=0){
        for(size_t i=0;i<line_count;i++)if(lines[i].entry==journal_jump){spread=(int)i/(2*lines_per_page);break;}
        journal_jump=-1;
    }
    if(spread>=spread_count) spread=spread_count-1;
    if(spread<0) spread=0;
    dirty=false;
}

static void update_geometry(void)
{
    int w,h;
    SDL_GetCurrentRenderOutputSize(renderer,&w,&h);
    float next_density=(float)h/900.0f;
    if(next_density<0.5f) next_density=0.5f;
    if(w!=output_width || h!=output_height || fonts[1].size!=body_size) {
        output_width=w;output_height=h;density=next_density;
        canvas_w=w/density;canvas_h=h/density;
        make_font(&fonts[0],book_font_data,14);
        make_font(&fonts[1],book_font_data,(float)body_size);
        make_font(&fonts[2],heading_font_data,22);
        make_font(&fonts[3],heading_font_data,30);
        dirty=true;
    }
    float width=fminf(1270,canvas_w-56);
    panel=(SDL_FRect){(canvas_w-width)/2,90,width,650};
    book=(SDL_FRect){panel.x+219,panel.y+65,panel.w-251,panel.h-118};
    text_width=(book.w-104)/2-70;
    lines_per_page=(int)((book.h-115)/(body_size*1.48f));
    if(lines_per_page<1) lines_per_page=1;
}

static void draw_journal(void)
{
    notebook_sync();journal_target_count=0;
    fill((SDL_FRect){0,0,canvas_w,canvas_h},(SDL_Color){8,6,4,160});
    frame(panel);
    text_draw(3,"Journal",panel.x+26,panel.y+17,GOLD);
    text_draw(0,"CARNET DE VOYAGE",panel.x+27,panel.y+56,MUTED);
    for(int i=0;i<7;i++) {
        SDL_FRect r={panel.x+21,panel.y+100+i*57,180,48};
        button(r,100+i,chapter_labels[i],chapter_art[i],chapter==i,true);
    }
    rule(panel.x+27,panel.y+520,163,(SDL_Color){121,86,44,255});
    text_draw(0,"Taille du texte",panel.x+28,panel.y+535,MUTED);
    button((SDL_FRect){panel.x+28,panel.y+565,64,34},202,"A-",-1,false,body_size>16);
    button((SDL_FRect){panel.x+103,panel.y+565,64,34},203,"A+",-1,false,body_size<26);
    button((SDL_FRect){panel.x+panel.w-49,panel.y+16,30,30},200,"X",-1,false,true);
    SDL_FRect sr={book.x+18,panel.y+20,book.w-80,33};
    fill(sr,(SDL_Color){19,14,10,255});outline(sr,searching?GOLD:(SDL_Color){113,85,47,255});
    text_draw(0,*search?search:"Rechercher dans cette rubrique...",sr.x+12,sr.y+6,*search?GOLD:MUTED);
    if(searching) {
        float caret=sr.x+12+text_measure(0,search);
        if((SDL_GetTicks()/450)%2==0) fill((SDL_FRect){caret,sr.y+7,1,18},GOLD);
    }
    buttons[button_count++]=(Button){sr,201,true};
    /* The original blank spread supplies its binding and worn parchment edges. */
    draw_art(261,book,false);
    float gutter=book.x+book.w/2;
    SDL_FRect pages[2]={{book.x+30,book.y+22,book.w/2-35,book.h-40},
        {gutter+5,book.y+22,book.w/2-35,book.h-40}};
    for(int i=0;i<2;i++) {
        material(paper_texture,pages[i]);
        for(int j=0;j<25;j++) {
            float gx = i == 0 ? pages[i].x+pages[i].w-25+j : pages[i].x+j;
            fill((SDL_FRect){gx,pages[i].y,1,pages[i].h},
                (SDL_Color){60,39,21,(Uint8)((i==0?j:24-j)*2)});
        }
        for(int j=0;j<6;j++) {
            rule(pages[i].x,pages[i].y+j,pages[i].w,(SDL_Color){87,62,30,(Uint8)(35-j*5)});
            rule(pages[i].x,pages[i].y+pages[i].h-j,pages[i].w,(SDL_Color){87,62,30,(Uint8)(35-j*5)});
        }
    }
    centered(2,chapters[chapter],pages[0].x+pages[0].w/2,book.y+31,INK);
    centered(2,chapters[chapter],pages[1].x+pages[1].w/2,book.y+31,INK);
    rule(pages[0].x+24,book.y+65,pages[0].w-48,(SDL_Color){137,115,75,150});
    rule(pages[1].x+24,book.y+65,pages[1].w-48,(SDL_Color){137,115,75,150});
    if(dirty) rebuild_book();
    for(int side=0;side<2;side++) {
        int start=(spread*2+side)*lines_per_page;
        for(int row=0;row<lines_per_page && start+row<(int)line_count;row++) {
            BookLine* l=&lines[start+row];
            SDL_Color c=INK;
            if(l->heading) c=(SDL_Color){114,76,37,255};
            else if(l->style==1) c=(SDL_Color){34,63,78,255};
            else if(l->style==2) c=(SDL_Color){49,84,47,255};
            else if(l->style==3) c=(SDL_Color){121,44,36,255};
            float x=pages[side].x+22,y=book.y+80+row*body_size*1.48f;
            text_draw(l->heading?0:1,l->text,x,y,c);
            if(l->entry_start && l->entry>=0 && journal_target_count<64){
                int index=journal_target_count++;journal_targets[index]=(JournalMark){chapter,l->entry};
                float bx=pages[side].x+pages[side].w-57;
                button((SDL_FRect){bx,y,23,23},2000+index*2,"*",-1,journal_mark_index(chapter,l->entry)>=0,true);
                if(chapter==1)button((SDL_FRect){bx+26,y,23,23},2001+index*2,"o",-1,tracked_quest==l->entry,l->style==1);
            }
            if(l->style==4 && !l->heading) rule(x,y+body_size*.65f,text_measure(1,l->text),c);
        }
        char number[32];snprintf(number,sizeof(number),"- %d -",spread*2+side+1);
        centered(0,number,pages[side].x+pages[side].w/2,book.y+book.h-40,INK);
    }
    button((SDL_FRect){book.x+20,panel.y+panel.h-44,110,28},204,"< Précédentes",-1,false,spread>0);
    button((SDL_FRect){book.x+book.w-130,panel.y+panel.h-44,110,28},205,"Suivantes >",-1,false,spread+1<spread_count);
    char footer[100];snprintf(footer,sizeof(footer),"%d entrée%s   ·   %d / %d",matched_count,matched_count>1?"s":"",spread+1,spread_count);
    centered(0,footer,book.x+book.w/2,panel.y+panel.h-40,GOLD);
    float filter_y=panel.y+panel.h+14;
    button((SDL_FRect){book.x+book.w-155,filter_y,140,30},2300,tr("Favoris (*)","Favorites (*)"),-1,journal_favorites,true);
    if(chapter==1) {
        const char* filters[]={tr("Toutes","All"),tr("En cours","In progress"),game_text(modern_logbook_state_label(QUEST_STATE_COMPLETED)),game_text(modern_logbook_state_label(QUEST_STATE_BOTCHED)),game_text(modern_logbook_state_label(QUEST_STATE_MENTIONED))};
        float width=(book.w-190)/5;
        for(int i=0;i<5;i++)button((SDL_FRect){book.x+15+i*width,filter_y,width-6,30},210+i,filters[i],-1,quest_filter==i,true);
    }
}

/* Geometry is drawn at output resolution: no cropped, stretched HUD bitmap. */
static void ellipse(float cx,float cy,float rx,float ry,SDL_Color color)
{
    const int segments=64;
    SDL_Vertex vertices[66];int indices[192];
    SDL_FColor c={color.r/255.f,color.g/255.f,color.b/255.f,color.a/255.f};
    vertices[0]=(SDL_Vertex){{cx,cy},c,{0,0}};
    for(int i=0;i<=segments;i++) {
        float t=i*6.2831853f/segments;
        vertices[i+1]=(SDL_Vertex){{cx+cosf(t)*rx,cy+sinf(t)*ry},c,{0,0}};
        if(i<segments){indices[i*3]=0;indices[i*3+1]=i+1;indices[i*3+2]=i+2;}
    }
    SDL_RenderGeometry(renderer,NULL,vertices,segments+2,indices,segments*3);
}
static void gauge(float x,float y,int value,int maximum,bool fatigue)
{
    float cx=x+20,top=y+9,h=68;
    float amount=fmaxf(0,fminf(1,maximum>0?(float)value/maximum:0));
    ellipse(cx,top+h+8,26,7,(SDL_Color){0,0,0,150});
    /* Turned brass shoulders, a dark glass cavity and slender retaining rods. */
    fill((SDL_FRect){cx-22,top-1,44,h+3},(SDL_Color){25,19,15,255});
    for(int i=0;i<40;i++) {
        float sheen=sinf(i/39.f*3.14159f);
        fill((SDL_FRect){cx-20+i,top,1,h},(SDL_Color){(Uint8)(14+sheen*10),(Uint8)(17+sheen*12),(Uint8)(21+sheen*16),255});
    }
    float liquid=top+h-h*amount;
    for(int i=0;i<36;i++) {
        float sheen=powf(sinf(i/35.f*3.14159f),.65f);
        SDL_Color color=fatigue?(SDL_Color){(Uint8)(13+18*sheen),(Uint8)(38+60*sheen),(Uint8)(90+110*sheen),255}:
            (SDL_Color){(Uint8)(65+125*sheen),(Uint8)(10+26*sheen),(Uint8)(17+25*sheen),255};
        fill((SDL_FRect){cx-18+i,liquid,1,h*amount},color);
    }
    if(amount>0)ellipse(cx,liquid,18,2,fatigue?(SDL_Color){96,164,226,255}:(SDL_Color){227,100,97,255});
    fill((SDL_FRect){cx-13,top+3,2,h-6},(SDL_Color){237,241,230,95});
    fill((SDL_FRect){cx-9,top+3,1,h-6},(SDL_Color){237,241,230,35});
    for(int side=-1;side<=1;side+=2) {
        fill((SDL_FRect){cx+side*22-2,top,4,h},(SDL_Color){78,56,29,255});
        fill((SDL_FRect){cx+side*22-1,top,1,h},(SDL_Color){215,185,111,255});
    }
    for(int end=0;end<2;end++) {
        float yy=end?top+h-1:top-7;
        for(int i=0;i<8;i++) {
            int light=i<3?160+i*24:220-(i-3)*32;
            fill((SDL_FRect){cx-24,yy+i,48,1},(SDL_Color){(Uint8)light,(Uint8)(light*.78f),(Uint8)(light*.41f),255});
        }
        ellipse(cx,yy,24,3,(SDL_Color){178,141,73,255});
    }
    ellipse(cx,top-10,12,3,(SDL_Color){125,91,44,255});
    fill((SDL_FRect){cx-10,top-14,20,4},(SDL_Color){175,140,73,255});
    char number[40];snprintf(number,sizeof(number),"%d / %d",value,maximum);
    SDL_FRect plaque={cx-35,y+87,70,21};fill(plaque,(SDL_Color){19,14,10,255});outline(plaque,(SDL_Color){119,89,45,255});
    centered(0,number,cx,y+89,GOLD);
}

static void draw_hud(void)
{
    bool controls_enabled=intgame_mode_get()!=INTGAME_MODE_DIALOG;
    int icons[5];for(int i=0;i<5;i++)icons[i]=modern_intgame_icon(i);
    const int actions[]={1,2,0,3,4};
    const char* labels[]={game_text(modern_intgame_text(1005)),game_text(modern_intgame_text(1006)),game_text(modern_intgame_text(1004)),game_text(modern_intgame_text(1007)),game_text(modern_intgame_text(1008))};
    float top_width=930,tx=(canvas_w-top_width)/2;
    frame((SDL_FRect){tx,10,top_width,60});
    char fate_label[48];snprintf(fate_label,sizeof(fate_label),"%s : %d",game_text(stat_name(STAT_FATE_POINTS)),stat_level_get(player_get_local_pc_obj(),STAT_FATE_POINTS));
    for(int i=0;i<5;i++) button((SDL_FRect){tx+13+i*182,20,175,40},actions[i],i==4?fate_label:labels[i],icons[i],false,controls_enabled);
    float w=980,x=(canvas_w-w)/2,y=canvas_h-126;
    frame((SDL_FRect){x,y,w,116});
    int64_t pc=player_get_local_pc_obj();
    gauge(x+27,y+9,object_hp_current(pc),object_hp_max(pc),false);
    gauge(x+w-65,y+9,critter_fatigue_current(pc),critter_fatigue_max(pc),true);
    float sx=x+102;
    for(int i=0;i<10;i++) {
        SDL_FRect slot={sx+i*49,y+15,43,44};
        button(slot,300+i,"",-1,false,controls_enabled);
        Hotkey* hk=sub_57F240(i);
        if(hk && hk->type!=HOTKEY_NONE) draw_art_id(hk->art_id,(SDL_FRect){slot.x+5,slot.y+5,33,31},true);
        char key[8];snprintf(key,sizeof(key),"%d",(i+1)%10);
        text_draw(0,key,slot.x+3,slot.y+28,MUTED);
    }
    const int small_icons[]={470,473,472,471,137};
    const int small_actions[]={7,6,5,8,9};
    const char* small_labels[]={game_text(modern_intgame_text(1002)),game_text(modern_intgame_text(1000)),game_text(modern_intgame_text(1001)),game_text(modern_intgame_text(1003)),game_text(modern_intgame_text(1009))};
    for(int i=0;i<5;i++) {
        SDL_FRect r={x+600+i*62,y+12,53,48};
        button(r,small_actions[i],"",small_icons[i],false,controls_enabled);
        text_compact(small_labels[i],r.x-3,y+62,r.w+6,true,MUTED);

    }
    rule(sx,y+69,480,(SDL_Color){117,87,44,255});
    if(*message_text && SDL_GetTicks()-message_time<12000) {
        char short_text[512];SDL_strlcpy(short_text,message_text,sizeof(short_text));
        while(*short_text && text_measure(0,short_text)>475) {
            size_t n=strlen(short_text);do {n--;} while(n>0 && ((unsigned char)short_text[n]&0xC0)==0x80);
            short_text[n]=0;
        }
        text_draw(0,short_text,sx,y+79,GOLD);
    } else {
        char status[256];
        snprintf(status,sizeof(status),"Niv. %d   ·   Or : %d   ·   Exp. : %d   ·   %02d:%02d",
            stat_level_get(pc,STAT_LEVEL),item_gold_get(pc),stat_level_get(pc,STAT_EXPERIENCE_POINTS),
            datetime_current_hour(),datetime_current_minute());
        int64_t weapon=item_wield_get(pc,ITEM_INV_LOC_WEAPON);
        if(weapon!=OBJ_HANDLE_NULL && item_weapon_ammo_type(weapon)!=10000) {
            char ammo[40];snprintf(ammo,sizeof(ammo),"   ·   Munitions : %d",item_ammo_quantity_get(pc,item_weapon_ammo_type(weapon)));
            SDL_strlcat(status,ammo,sizeof(status));
        }
        text_draw(0,status,sx,y+79,MUTED);
    }
}

static bool visible(void)
{
    return enabled && in_game && !mainmenu_ui_is_active() && player_get_local_pc_obj()!=OBJ_HANDLE_NULL;
}

static bool native_menu(void)
{
    if (enabled && confirming_menu) return true;
    if (!enabled || menu_suspended || !mainmenu_ui_is_active()) return false;
    if (slide_ui_is_active()) return false;
    MainMenuWindowType type = mainmenu_ui_window_type_get();
    return type == MM_WINDOW_NEW_CHAR || type == MM_WINDOW_PREGEN_CHAR
        || type == MM_WINDOW_LOAD_GAME || type == MM_WINDOW_SAVE_GAME || type == MM_WINDOW_CHAREDIT
        || type == MM_WINDOW_MAINMENU || type == MM_WINDOW_SINGLE_PLAYER
        || type == MM_WINDOW_PICK_NEW_OR_PREGEN || type == MM_WINDOW_26
        || type == MM_WINDOW_MAINMENU_IN_PLAY || type == MM_WINDOW_MAINMENU_IN_PLAY_LOCKED
        || type == MM_WINDOW_OPTIONS;
}

bool modern_ui_menu_suspend(bool suspend)
{
    bool previous=menu_suspended;
    if(suspend) modern_graphics_cancel();
    menu_suspended=suspend;
    return previous;
}

static void menu_button(SDL_FRect rect, int action, const char* label, bool available)
{
    button(rect, action, "", -1, false, available);
    if(control_symbol(rect,label,available,inside(rect,mouse_x,mouse_y)))return;
    /* Center the visible glyphs, including accents, in buttons of any height. */
    NativeFont* font=&fonts[2];
    float top=font->size,bottom=0;
    for(const char* p=label;*p;) {
        int c=codepoint(&p);stbtt_packedchar* g=&font->glyphs[c-GLYPH_FIRST];
        if(g->y1>g->y0){top=fminf(top,font->size+g->yoff/font->density);bottom=fmaxf(bottom,font->size+g->yoff2/font->density);}
    }
    centered(2,label,rect.x+rect.w/2,rect.y+(rect.h-bottom-top)/2,available?GOLD:MUTED);
}

static void draw_menu_background(void)
{
    fill((SDL_FRect){0,0,canvas_w,canvas_h}, (SDL_Color){0,0,0,255});
    if (menu_background) {
        float scale=fminf(canvas_w/menu_background_width,canvas_h/menu_background_height);
        SDL_FRect rect={(canvas_w-menu_background_width*scale)/2,
            (canvas_h-menu_background_height*scale)/2,
            menu_background_width*scale,menu_background_height*scale};
        SDL_RenderTexture(renderer,menu_background,NULL,&rect);
    }
}

/* Localized engine strings are converted only at the presentation boundary. */
static const char* game_text(const char* source)
{
    static char buffers[16][8192];
    static unsigned next;
    char* result=buffers[next++%16];
    result[0]=0;from_game(source,result,8192);
    return result;
}

static const char* menu_text(int number)
{
    MesFileEntry entry={0};entry.num=number;
    mes_get_msg(mainmenu_ui_mes_file(),&entry);
    return game_text(entry.str);
}

static const char* character_text(int number)
{
    const char* result=game_text(modern_charedit_text(number));
    char* newline=strpbrk((char*)result,"\r\n");
    if(newline) *newline=0;
    return result;
}

static bool name_focused, name_replace, derived_stats;
static int description_tab=2, description_scroll, description_max;
static int save_page, sheet_category, sheet_page, sheet_selected;
static SDL_Texture *portrait_texture, *save_texture;
static int portrait_number=-1, thumbnail_index=-2;
static float portrait_width, portrait_height, thumbnail_width, thumbnail_height;

static SDL_Texture* texture_from_buffer(TigVideoBuffer* buffer, float* width, float* height)
{
    if(!buffer || tig_video_buffer_lock(buffer)!=TIG_OK) return NULL;
    TigVideoBufferData data;
    SDL_Texture* texture=NULL;
    if(tig_video_buffer_data(buffer,&data)==TIG_OK && data.width>0 && data.height>0) {
        Uint32* pixels=malloc((size_t)data.width*data.height*sizeof(*pixels));
        if(pixels) {
            for(int y=0;y<data.height;y++) for(int x=0;x<data.width;x++)
                pixels[y*data.width+x]=((Uint32*)((unsigned char*)data.pixels+y*data.pitch))[x]|0xFF000000;
            texture=rgba_texture(data.width,data.height,pixels);
            *width=data.width;*height=data.height;free(pixels);
        }
    }
    tig_video_buffer_unlock(buffer);
    return texture;
}

static void fitted_texture(SDL_Texture* texture,float width,float height,SDL_FRect rect)
{
    if(!texture || width<=0 || height<=0) return;
    float scale=fminf(rect.w/width,rect.h/height);
    rect.x+=(rect.w-width*scale)/2;rect.y+=(rect.h-height*scale)/2;
    rect.w=width*scale;rect.h=height*scale;
    SDL_RenderTexture(renderer,texture,NULL,&rect);
}

static void draw_portrait(int number,SDL_FRect rect)
{
    if(number!=portrait_number) {
        SDL_DestroyTexture(portrait_texture);portrait_texture=NULL;portrait_number=number;
        char path[260];portrait_path(number,path,sizeof(path),128);
        TigVideoBuffer* buffer=NULL;
        int result=*path ? tig_video_buffer_load_from_bmp(path,&buffer,1) : -1;
        if(result!=TIG_OK) {
            portrait_path(number,path,sizeof(path),64);
            result=*path ? tig_video_buffer_load_from_bmp(path,&buffer,1) : -1;
        }
        if(result==TIG_OK) {
            portrait_texture=texture_from_buffer(buffer,&portrait_width,&portrait_height);
            tig_video_buffer_destroy(buffer);
        }
    }
    fill(rect,(SDL_Color){17,12,9,255});outline(rect,GOLD);
    rect.x+=5;rect.y+=5;rect.w-=10;rect.h-=10;
    fitted_texture(portrait_texture,portrait_width,portrait_height,rect);
}

/* Wrap at Unicode character boundaries, preserving text and explicit newlines.
 * Return the full line count so long translations remain reachable by scrolling. */
static int menu_paragraph_aligned(const char* text,SDL_FRect rect,int offset,SDL_Color color,bool align_center)
{
    int row=0, visible_rows=(int)(rect.h/27);
    while(*text) {
        while(*text==' ' || *text=='\t' || *text=='\r') text++;
        if(!*text) break;
        const char *start=text,*scan=text,*space=NULL;
        float width=0;char line[1024];
        while(*scan && *scan!='\n') {
            const char* before=scan;int c=codepoint(&scan);
            float next=fonts[1].glyphs[c-GLYPH_FIRST].xadvance/fonts[1].density;
            if((width+next>rect.w || scan-start>1000) && before>start) {scan=space && space>start ? space : before;break;}
            if(*before==' ') space=before;
            width+=next;
        }
        size_t length=(size_t)(scan-start);
        memcpy(line,start,length);line[length]=0;
        if(row>=offset && row<offset+visible_rows) {
            float x=align_center?rect.x+(rect.w-text_measure(1,line))/2:rect.x;
            text_draw(1,line,x,rect.y+(row-offset)*27,color);
        }
        row++;text=scan;
        if(*text=='\n') text++;
    }
    return row;
}

static int menu_paragraph(const char* text,SDL_FRect rect,int offset,SDL_Color color)
{return menu_paragraph_aligned(text,rect,offset,color,false);}

static void description_panel(const char* text,SDL_FRect rect)
{
    fill(rect,(SDL_Color){20,14,10,210});outline(rect,(SDL_Color){116,85,43,255});
    SDL_FRect content={rect.x+20,rect.y+16,rect.w-82,rect.h-28};
    int rows=menu_paragraph(text,content,description_scroll,GOLD);
    description_max=rows-(int)(content.h/27);if(description_max<0)description_max=0;
    if(description_scroll>description_max)description_scroll=description_max;
    menu_button((SDL_FRect){rect.x+rect.w-49,rect.y+12,36,46},540,"^",description_scroll>0);
    menu_button((SDL_FRect){rect.x+rect.w-49,rect.y+rect.h-58,36,46},541,"v",description_scroll<description_max);
}

static void text_in_button(SDL_FRect rect,int action,const char* label,bool selected,bool available)
{
    button(rect,action,"",-1,selected,available);
    int font=text_measure(2,label)<=rect.w-24 ? 2 : 0;
    /* Very long names use a second line instead of leaking into adjacent controls. */
    if(text_measure(font,label)<=rect.w-24) centered(font,label,rect.x+rect.w/2,rect.y+10,available?GOLD:MUTED);
    else menu_paragraph(label,(SDL_FRect){rect.x+12,rect.y+4,rect.w-24,rect.h-6},0,available?GOLD:MUTED);
}

#include "modern_keys.inc"

static void draw_character_menu(void)
{
    ModernCharacter c;if(!modern_mainmenu_character(&c,description_tab))return;
    float w=fminf(1360,canvas_w-40),h=800,x=(canvas_w-w)/2,y=(canvas_h-h)/2;
    frame((SDL_FRect){x,y,w,h});
    centered(3,menu_text(c.editable?421:420),canvas_w/2,y+22,GOLD);
    rule(x+30,y+75,w-60,MUTED);
    float left=x+30,lw=360,right=left+lw+35,rw=w-lw-95;
    draw_portrait(c.portrait,(SDL_FRect){left+98,y+97,164,164});
    menu_button((SDL_FRect){left,y+153,66,52},502,"<",true);
    menu_button((SDL_FRect){left+lw-66,y+153,66,52},503,">",true);
    if(!c.editable) {
        char count[32];snprintf(count,sizeof(count),"%d / %d",c.index,c.count);
        centered(0,count,left+lw/2,y+269,MUTED);
    }
    char name[160];SDL_strlcpy(name,game_text(c.name),sizeof(name));
    if(name_focused && !name_replace && (SDL_GetTicks()/500)%2==0)SDL_strlcat(name," |",sizeof(name));
    text_in_button((SDL_FRect){left,y+303,lw,58},522,name,name_focused,c.editable);
    if(c.editable) text_draw(0,menu_text(2210),left,y+278,MUTED);
    text_draw(0,character_text(5),left,y+386,MUTED);
    text_in_button((SDL_FRect){left+48,y+412,lw-96,52},530,game_text(c.gender),description_tab==0,true);
    if(c.editable) {
        menu_button((SDL_FRect){left,y+412,42,52},520,"<",c.gender_editable);
        menu_button((SDL_FRect){left+lw-42,y+412,42,52},521,">",c.gender_editable);
    }
    text_draw(0,menu_text(745),left,y+478,MUTED);
    text_in_button((SDL_FRect){left+48,y+504,lw-96,52},531,game_text(c.race),description_tab==1,true);
    if(c.editable) {
        menu_button((SDL_FRect){left,y+504,42,52},504,"<",true);
        menu_button((SDL_FRect){left+lw-42,y+504,42,52},505,">",true);
    }
    text_draw(0,menu_text(800),left,y+570,MUTED);
    text_in_button((SDL_FRect){left+48,y+596,lw-96,72},532,game_text(c.background),description_tab==2,true);
    if(c.editable) {
        menu_button((SDL_FRect){left,y+606,42,52},506,"<",true);
        menu_button((SDL_FRect){left+lw-42,y+606,42,52},507,">",true);
    }
    text_in_button((SDL_FRect){right,y+92,rw/2-8,44},533,menu_text(739),!derived_stats,true);
    text_in_button((SDL_FRect){right+rw/2,y+92,rw/2,44},534,character_text(7),derived_stats,true);
    const int stats[]={STAT_STRENGTH,STAT_CONSTITUTION,STAT_DEXTERITY,STAT_BEAUTY,
        STAT_INTELLIGENCE,STAT_WILLPOWER,STAT_PERCEPTION,STAT_CHARISMA};
    for(int i=0;i<8;i++) {
        float sx=right+(i/4)*rw/2,sy=y+145+(i%4)*39;
        int stat=derived_stats?STAT_CARRY_WEIGHT+i:stats[i];
        char value[32];snprintf(value,sizeof(value),"%d",stat_level_get(c.object,stat));
        text_draw(derived_stats?0:1,game_text(stat_name(stat)),sx,sy,GOLD);
        text_draw(2,value,sx+rw/2-55,sy,GOLD);
    }
    const char* heading=c.description_kind==0?character_text(5):c.description_kind==1?menu_text(745):menu_text(800);
    text_draw(2,heading,right,y+325,GOLD);
    description_panel(game_text(c.description),(SDL_FRect){right,y+368,rw,300});
    menu_button((SDL_FRect){left,y+716,280,54},501,menu_text(7002),true);
    bool valid=!c.editable || (*c.name && strcmp(game_text(c.name),menu_text(500))!=0);
    menu_button((SDL_FRect){x+w-310,y+716,280,54},500,menu_text(7003),valid);
}

static void draw_load_menu(void)
{
    bool saving=mainmenu_ui_window_type_get()==MM_WINDOW_SAVE_GAME;
    MainMenuWindowInfo* info=sub_5496C0(saving?MM_WINDOW_SAVE_GAME:MM_WINDOW_LOAD_GAME);
    float w=fminf(1360,canvas_w-40),h=800,x=(canvas_w-w)/2,y=(canvas_h-h)/2;
    frame((SDL_FRect){x,y,w,h});
    centered(3,menu_text(saving?7021:7020),canvas_w/2,y+22,GOLD);
    rule(x+30,y+75,w-60,MUTED);
    float left=x+30,lw=w*.39f,right=left+lw+30,rw=w-lw-90;
    int pages=(info->cnt+7)/8;if(pages<1)pages=1;
    if(save_page>=pages)save_page=pages-1;
    for(int row=0;row<8;row++) {
        int index=save_page*8+row;if(index>=info->cnt)break;
        text_in_button((SDL_FRect){left,y+104+row*62,lw,54},550+row,
            game_text(modern_mainmenu_save_name(index)),info->selected_index==index,true);
    }
    if(!info->cnt) centered(1,menu_text(5005),left+lw/2,y+290,MUTED);
    menu_button((SDL_FRect){left,y+632,65,48},560,"<",save_page>0);
    menu_button((SDL_FRect){left+lw-65,y+632,65,48},561,">",save_page+1<pages);
    char count[64];snprintf(count,sizeof(count),"%d / %d",save_page+1,pages);
    centered(0,count,left+lw/2,y+645,MUTED);
    const GameSaveInfo* save=modern_mainmenu_save_info();
    if(thumbnail_index!=info->selected_index) {
        SDL_DestroyTexture(save_texture);save_texture=NULL;thumbnail_index=info->selected_index;
        if(save)save_texture=texture_from_buffer(save->thumbnail_video_buffer,&thumbnail_width,&thumbnail_height);
    }
    SDL_FRect preview={right,y+104,rw,300};
    if(saving && info->selected_index==0) {
        int64_t pc=player_get_local_pc_obj();
        if(pc) {
            draw_portrait(portrait_get(pc),(SDL_FRect){right+rw/2-100,y+110,200,200});
            char name[2048];object_examine(pc,pc,name);
            centered(2,game_text(name),right+rw/2,y+334,GOLD);
            char level[160];snprintf(level,sizeof(level),"%s : %d",game_text(stat_name(STAT_LEVEL)),stat_level_get(pc,STAT_LEVEL));
            centered(1,level,right+rw/2,y+374,MUTED);
        }
    } else {
        fill(preview,(SDL_Color){15,10,8,255});
        fitted_texture(save_texture,thumbnail_width,thumbnail_height,preview);outline(preview,MUTED);
    }
    if(save) {
        text_draw(2,game_text(save->pc_name),right,y+422,GOLD);
        char level[160],date[80],time[80];
        snprintf(level,sizeof(level),"%s : %d",game_text(stat_name(STAT_LEVEL)),save->pc_level);
        text_draw(1,level,right,y+462,GOLD);
        text_draw(1,game_text(modern_mainmenu_save_location()),right,y+498,GOLD);
        DateTime saved_time=save->datetime;
        datetime_format_date(&saved_time,date,sizeof(date));datetime_format_time(&saved_time,time,sizeof(time));
        char when[180];snprintf(when,sizeof(when),"%s  %s",date,time);
        text_draw(0,game_text(when),right,y+535,MUTED);
        menu_paragraph(game_text(save->description),(SDL_FRect){right,y+574,rw,81},0,GOLD);
        if(!saving && save->version!=25)text_draw(1,menu_text(5004),right,y+666,(SDL_Color){230,120,95,255});
    }
    if(saving && info->selected_index==0) {
        text_draw(1,menu_text(2210),right,y+434,GOLD);
        SDL_FRect field={right,y+480,rw,58};
        char name[160];SDL_strlcpy(name,game_text(modern_mainmenu_save_description()),sizeof(name));
        if(name_focused && (SDL_GetTicks()/500)%2==0)SDL_strlcat(name," |",sizeof(name));
        text_in_button(field,522,name,name_focused,true);
    }
    menu_button((SDL_FRect){left,y+716,230,54},501,menu_text(54),true);
    text_in_button((SDL_FRect){x+w/2-220,y+716,440,54},502,menu_text(7022),false,info->selected_index>=(saving?1:0));
    menu_button((SDL_FRect){x+w-260,y+716,230,54},500,menu_text(saving?7021:7020),saving?info->selected_index>=0:save && save->version==25);
}

static const int sheet_stats[]={-1,-1,STAT_STRENGTH,STAT_CONSTITUTION,STAT_DEXTERITY,STAT_BEAUTY,
    STAT_INTELLIGENCE,STAT_WILLPOWER,STAT_PERCEPTION,STAT_CHARISMA};
static const int sheet_skills[]={BASIC_SKILL_BOW,BASIC_SKILL_DODGE,BASIC_SKILL_MELEE,BASIC_SKILL_THROWING,
    BASIC_SKILL_BACKSTAB,BASIC_SKILL_PICK_POCKET,BASIC_SKILL_PROWLING,BASIC_SKILL_SPOT_TRAP,
    BASIC_SKILL_GAMBLING,BASIC_SKILL_HAGGLE,BASIC_SKILL_HEAL,BASIC_SKILL_PERSUATION,
    TECH_SKILL_REPAIR,TECH_SKILL_FIREARMS,TECH_SKILL_PICK_LOCKS,TECH_SKILL_DISARM_TRAPS};

static void sheet_item(int64_t obj,int category,int index,const char** name,const char** description,int* value)
{
    if(category==0) {
        *name=index<2?character_text(106+index):game_text(stat_name(sheet_stats[index]));
        *description=game_text(modern_charedit_text(index<2?106+index:107+index));
        *value=index==0?object_hp_max(obj):index==1?critter_fatigue_max(obj):stat_level_get(obj,sheet_stats[index]);
    } else if(category==1) {
        int skill=sheet_skills[index];
        *name=game_text(index<12?basic_skill_name(skill):tech_skill_name(skill));
        *description=game_text(index<12?basic_skill_description(skill):tech_skill_description(skill));
        *value=index<12?basic_skill_points_get(obj,skill):tech_skill_points_get(obj,skill);
    } else if(category==2) {
        *name=game_text(tech_discipline_name_get(index));*description=game_text(tech_discipline_description_get(index));
        *value=tech_degree_get(obj,index);
    } else {
        *name=game_text(spell_college_name(index));*description=game_text(spell_college_description(index));
        *value=spell_college_level_get(obj,index);
    }
}

#include "modern_sheet.inc"

static void draw_confirmation(void)
{
    float w=fminf(850,canvas_w-60), x=(canvas_w-w)/2, y=(canvas_h-340)/2;
    frame((SDL_FRect){x,y,w,340});
    centered(3,"Arcanum",canvas_w/2,y+28,GOLD);
    /* Wrap locally, without changing the journal's pagination. */
    char remaining[512];
    const char* cursor=confirmation_text;
    float line_y=y+99;
    while (*cursor && line_y<y+215) {
        SDL_strlcpy(remaining,cursor,sizeof(remaining));
        size_t end=strlen(remaining);
        while (end>0 && text_measure(1,remaining)>w-80) {
            char* space=strrchr(remaining,' ');
            if (!space) break;
            end=(size_t)(space-remaining);*space=0;
        }
        centered(1,remaining,canvas_w/2,line_y,GOLD);
        cursor+=end;
        while (*cursor==' ') cursor++;
        line_y+=31;
    }
    menu_button((SDL_FRect){x+40,y+251,(w-100)/2,54},450,menu_text(260),true);
    menu_button((SDL_FRect){x+60+(w-100)/2,y+251,(w-100)/2,54},451,menu_text(54),true);
}

bool modern_ui_confirm(const char* message)
{
    modern_graphics_cancel();
    from_game(message,confirmation_text,sizeof(confirmation_text));
    confirmation_choice=-1;confirming_menu=true;
    pending_action=-1;button_count=0;pressed_action=-1;click_captured=false;
    while (confirmation_choice<0) {
        tig_ping();
        if (pending_action==450 || pending_action==451) {
            confirmation_choice=pending_action==450;
            pending_action=-1;
        }
        TigMessage msg;
        while (tig_message_dequeue(&msg)==TIG_OK) {
            if (msg.type==TIG_MESSAGE_QUIT) confirmation_choice=0;
            if (msg.type==TIG_MESSAGE_REDRAW) gamelib_redraw();
        }
        tig_window_display();
        SDL_Delay(1);
    }
    confirming_menu=false;button_count=0;
    return confirmation_choice==1;
}

static void draw_menu(void)
{
    MainMenuWindowType type=mainmenu_ui_window_type_get();
    bool options = type == MM_WINDOW_OPTIONS;
    bool pause = type == MM_WINDOW_MAINMENU_IN_PLAY || type == MM_WINDOW_MAINMENU_IN_PLAY_LOCKED || type == MM_WINDOW_26;
    draw_menu_background();
    if (confirming_menu) {draw_confirmation();return;}
    if(type==MM_WINDOW_NEW_CHAR || type==MM_WINDOW_PREGEN_CHAR) {draw_character_menu();return;}
    if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME) {draw_load_menu();return;}
    if(type==MM_WINDOW_CHAREDIT) {draw_character_sheet();return;}
    float w = options ? 1180 : 610, h = options ? 760 : 660;
    float x = (canvas_w-w)/2, y = (canvas_h-h)/2;
    frame((SDL_FRect){x,y,w,h});
    centered(3, options ? menu_text(12) : "Arcanum", canvas_w/2, y+33, GOLD);
    const char* subtitle=options ? menu_text(12) : pause ? menu_text(4000) :
        type==MM_WINDOW_SINGLE_PLAYER ? menu_text(10) : type==MM_WINDOW_PICK_NEW_OR_PREGEN ? menu_text(50) : "";
    centered(0,subtitle,canvas_w/2,y+80,MUTED);
    rule(x+36,y+112,w-72,(SDL_Color){117,87,44,255});
    if (!options) {
        MainMenuWindowInfo* info=sub_5496C0(type);
        int count=info->num_buttons;
        float first=y+144+(5-count)*35;
        for (int row=0;row<count;row++) {
            int index=pause ? (row==0 ? count-1 : row-1) : row;
            MesFileEntry entry={0};entry.num=info->num+index+(type==MM_WINDOW_26 && index>=1 ? 1 : 0);
            mes_get_msg(mainmenu_ui_mes_file(),&entry);
            char label[256];from_game(entry.str,label,sizeof(label));
            if(pause && row==0) SDL_strlcpy(label,menu_text(34),sizeof(label));
            bool hidden=true;
            tig_button_is_hidden(info->buttons[index].button_handle,&hidden);
            menu_button((SDL_FRect){x+55,first+row*81,w-110,58},500+index,label,!hidden);
        }
        if(type!=MM_WINDOW_MAINMENU) centered(0,menu_text(pause?34:4003),canvas_w/2,y+h-46,MUTED);
        return;
    }
    ModernGraphics* g = &modern_graphics;
    bool confirming = g->deadline != 0;
    button((SDL_FRect){x+30,y+140,182,45},407,"Graphismes",-1,options_tab<0,!confirming);
    const int tab_actions[]={406,408,409};
    for(int i=0;i<3;i++)button((SDL_FRect){x+30,y+197+i*57,182,45},tab_actions[i],menu_text(4000+i),-1,options_tab==i,!confirming);
    button((SDL_FRect){x+30,y+368,182,45},418,tr("Raccourcis","Key bindings"),-1,options_tab==3,!confirming);
    if(options_tab==3){draw_key_options(x,y,w,h);return;}
    if(options_tab>=0) {
        float ox=x+246,ow=w-280,cw=(ow-24)/2;
        for(int i=0;i<8;i++) {
            ModernOption option;if(!modern_options_state(i,&option))continue;
            float cx=ox+(i/4)*(cw+24),cy=y+137+(i%4)*114;
            SDL_Color color=option.enabled?GOLD:MUTED;
            menu_paragraph(game_text(option.label),(SDL_FRect){cx,cy,cw,54},0,color);
            menu_button((SDL_FRect){cx,cy+57,38,42},430+i*2,"<",option.enabled && (!option.numeric || option.value>0));
            menu_button((SDL_FRect){cx+cw-38,cy+57,38,42},431+i*2,">",option.enabled && (!option.numeric || option.value<option.maximum));
            char value[80];snprintf(value,sizeof(value),"%d / %d",option.value,option.maximum);
            const char* label=option.text?game_text(option.text):value;
            /* Native text fitting handles long translated values without pixel scaling. */
            text_in_button((SDL_FRect){cx+44,cy+57,cw-88,42},-1,label,false,false);
            if(option.numeric && option.maximum>0) {
                fill((SDL_FRect){cx+47,cy+102,cw-94,3},(SDL_Color){60,45,27,255});
                fill((SDL_FRect){cx+47,cy+102,(cw-94)*option.value/option.maximum,3},color);
            }
        }
        if(options_tab==0) {
            bool on=settings_get_value(&settings,MODERN_UNLIMITED_FOLLOWERS_KEY)==1;
            bool french=strcmp(modern_intgame_text(0),"Niveau")==0;
            menu_paragraph(french?"Compagnons illimités":"Unlimited companions",(SDL_FRect){ox,y+601,ow-160,30},0,GOLD);
            MesFileEntry entry={.num=on?1:0};
            const char* value=on?"✓":"—";
            if(option_boolean_file!=MES_FILE_HANDLE_INVALID && mes_search(option_boolean_file,&entry))value=game_text(entry.str);
            menu_button((SDL_FRect){ox+ow-130,y+595,130,36},417,value,!tig_net_is_active());
            text_compact(french?"Retire la limite liée au charisme ; les conditions de recrutement restent actives.":"Removes the Charisma limit; recruitment requirements still apply.",ox,y+630,ow,false,MUTED);
        }
        rule(x+30,y+h-110,w-60,(SDL_Color){117,87,44,255});
        menu_button((SDL_FRect){x+40,y+h-82,220,54},405,menu_text(4003),true);
        return;
    }
    float rx=x+246, rw=w-280;
    text_draw(2,"Graphismes",rx,y+144,GOLD);
    text_draw(0,"Mode d'affichage",rx,y+207,MUTED);
    const char* modes[]={"Fenêtre", "Plein écran sans bordure", "Plein écran exclusif"};
    menu_button((SDL_FRect){rx,y+238,42,48},410,"<",!confirming);
    centered(2,modes[g->selected.mode],rx+rw/2,y+249,GOLD);
    menu_button((SDL_FRect){rx+rw-42,y+238,42,48},411,">",!confirming);
    text_draw(0,"Résolution",rx,y+319,MUTED);
    char resolution[64];
    snprintf(resolution,sizeof(resolution),"%d × %d",g->selected.width,g->selected.height);
    bool can_resize=!confirming && g->count>1 && g->selected.mode!=1;
    menu_button((SDL_FRect){rx,y+350,42,48},412,"<",can_resize);
    centered(2,g->count ? resolution : "Aucun mode disponible",rx+rw/2,y+361,GOLD);
    menu_button((SDL_FRect){rx+rw-42,y+350,42,48},413,">",can_resize);
    text_draw(0,g->selected.mode==1 ? "Le mode sans bordure suit la définition du bureau." :
        "Choisissez une définition adaptée à votre écran.",rx,y+416,MUTED);
    if (*g->status) text_draw(0,g->status,rx,y+455,GOLD);
    rule(x+30,y+h-157,w-60,(SDL_Color){117,87,44,255});
    if (confirming) {
        Uint64 now=SDL_GetTicks();
        int seconds=g->deadline>now ? (int)((g->deadline-now+999)/1000) : 0;
        char question[128];snprintf(question,sizeof(question),"Conserver ces réglages ? Retour automatique dans %d s",seconds);
        centered(0,question,canvas_w/2,y+h-133,GOLD);
        menu_button((SDL_FRect){x+60,y+h-92,350,52},415,"Conserver",true);
        menu_button((SDL_FRect){x+w-410,y+h-92,350,52},416,"Rétablir",true);
    } else {
        menu_button((SDL_FRect){x+40,y+h-102,220,54},405,menu_text(4003),true);
        menu_button((SDL_FRect){x+w-300,y+h-102,260,54},414,"Appliquer",g->count>0);
    }
}

static void menu_name_focus(bool focus)
{
    name_focused=focus;name_replace=focus;
    if(focus)SDL_StartTextInput(window);else SDL_StopTextInput(window);
}

static void menu_name_input(const char* utf8,bool backspace)
{
    bool saving=mainmenu_ui_window_type_get()==MM_WINDOW_SAVE_GAME;
    ModernCharacter c;
    if(!saving && (!modern_mainmenu_character(&c,description_tab) || !c.editable))return;
    char name[24];SDL_strlcpy(name,saving?modern_mainmenu_save_description():c.name,sizeof(name));
    if(name_replace)name[0]=0;
    size_t n=strlen(name);
    if(backspace) {if(n)name[--n]=0;}
    else {
        /* Convert through SDL's iconv backend; the save format stores 23 CP1252 bytes. */
        char* encoded=SDL_iconv_string("WINDOWS-1252","UTF-8",utf8,strlen(utf8)+1);
        if(encoded) {
            for(const unsigned char* p=(const unsigned char*)encoded;*p && n<23;p++)
                if(*p>=32 && *p!=127)name[n++]=(char)*p;
            name[n]=0;SDL_free(encoded);
        }
    }
    name_replace=false;
    if(saving)modern_mainmenu_save_description_set(name);else modern_mainmenu_name(name);
}

static bool creation_menu_action(int action)
{
    MainMenuWindowType type=mainmenu_ui_window_type_get();
    if(action!=522 && name_focused)menu_name_focus(false);
    if(action>=500 && action<=510) {
        if(name_focused)menu_name_focus(false);
        description_scroll=0;
        if(type==MM_WINDOW_NEW_CHAR) {
            if(action==504 || action==505)description_tab=1;
            if(action==506 || action==507)description_tab=2;
        }
        if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME)thumbnail_index=-2;
        modern_mainmenu_activate(action-500);button_count=0;return true;
    }
    if(action==522) {
        if(type==MM_WINDOW_NEW_CHAR || (type==MM_WINDOW_SAVE_GAME && sub_5496C0(type)->selected_index==0))menu_name_focus(!name_focused);
        return true;
    }
    if(action==520 || action==521) {
        modern_mainmenu_gender(action==520?-1:1);description_tab=0;description_scroll=0;return true;
    }
    if(action>=530 && action<=532) {description_tab=action-530;description_scroll=0;return true;}
    if(action==533 || action==534) {derived_stats=action==534;return true;}
    if(action==540 || action==541) {
        description_scroll+=action==540?-3:3;
        if(description_scroll<0)description_scroll=0;
        if(description_scroll>description_max)description_scroll=description_max;
        return true;
    }
    if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME) {
        if(action>=550 && action<558) {modern_mainmenu_save_select(save_page*8+action-550);thumbnail_index=-2;
            if(type==MM_WINDOW_SAVE_GAME && sub_5496C0(type)->selected_index==0)menu_name_focus(true);return true;}
        if(action==560 || action==561) {
            int pages=(sub_5496C0(type)->cnt+7)/8;
            save_page+=action==560?-1:1;
            if(save_page<0)save_page=0;if(save_page>=pages)save_page=pages>0?pages-1:0;
            return true;
        }
    }
    if(type==MM_WINDOW_CHAREDIT || play_sheet()) {
        if(action>=651 && action<=653) {
            sheet_category=action-650;sheet_page=sheet_selected=0;sheet_select_detail(sheet_category,0);return true;
        }
        if(action>=684 && action<=687) {
            sheet_page=action-684;sheet_selected=sheet_page*4;sheet_select_detail(1,sheet_selected);return true;
        }
        if(action>=690 && action<700) {sheet_select_detail(0,action-690);return true;}
        if(action>=620 && action<640) {
            int index=(action-620)/2;sheet_select_detail(0,index);message_text[0]=0;
            modern_charedit_adjust(0,index,action%2?1:-1);return true;
        }
        if(action>=660 && action<676) {
            sheet_selected=sheet_category==1?sheet_page*4+action-660:action-660;
            sheet_select_detail(sheet_category,sheet_selected);return true;
        }
        if(action>=676 && action<=682) {
            sheet_select_detail(sheet_category,sheet_selected);sheet_degree=action-676;return true;
        }
        if(action==640 || action==641) {
            sheet_select_detail(sheet_category,sheet_selected);message_text[0]=0;
            modern_charedit_adjust(sheet_category,sheet_selected,action==641?1:-1);return true;
        }
        if(action>=600 && action<608) {
            sheet_selected=sheet_page*4+(action-600)/2;
            sheet_select_detail(1,sheet_selected);message_text[0]=0;
            modern_charedit_adjust(1,sheet_selected,action%2?1:-1);return true;
        }
    }
    return false;
}

/* Zoom affects the world raster only; native fonts and controls retain their density. */
static bool hud_region(float x,float y);
static int zoom_base_width,zoom_base_height;
static float world_zoom=1,zoom_anchor_x=.5f,zoom_anchor_y=.5f;
static bool zoom_pending;
static bool world_zoom_allowed(void)
{
    int panel=native_panel(),mode=intgame_mode_get();
    return enabled && in_game && visible() && !mainmenu_ui_is_active() && !journal
        && (panel==0 || panel==5) && !intgame_is_compact_interface()
        && (mode==INTGAME_MODE_MAIN || mode==INTGAME_MODE_SPELL
            || mode==INTGAME_MODE_SKILL || mode==INTGAME_MODE_ITEM);
}
static void world_zoom_sync(void)
{
    if(!zoom_base_width) {
        zoom_base_width=hrp_iso_window_width_get();zoom_base_height=hrp_iso_window_height_get();
        if(!zoom_base_width || !zoom_base_height)return;
    }
    bool active=world_zoom_allowed();
    float zoom=active?world_zoom:1;
    int width=(int)lroundf(zoom_base_width/zoom),height=(int)lroundf(zoom_base_height/zoom);
    if(width==hrp_iso_window_width_get() && height==hrp_iso_window_height_get()) {zoom_pending=false;return;}
    float ax=active && zoom_pending?zoom_anchor_x:.5f,ay=active && zoom_pending?zoom_anchor_y:.5f;
    if(!modern_world_resize(width,height,ax,ay)) {
        world_zoom=(float)zoom_base_width/hrp_iso_window_width_get();
        SDL_Log("World zoom: unable to resize framebuffer");
    }
    zoom_pending=false;
}
static bool world_zoom_event(SDL_Event* event)
{
    if(event->type!=SDL_EVENT_MOUSE_WHEEL || !world_zoom_allowed() || native_panel())return false;
    int w,h;SDL_GetWindowSize(window,&w,&h);if(w<=0 || h<=0)return false;
    float x=event->wheel.mouse_x/w,y=event->wheel.mouse_y/h;
    if(hud_region(x*canvas_w,y*canvas_h))return true;
    if(SDL_GetMouseState(NULL,NULL)&(SDL_BUTTON_LMASK|SDL_BUTTON_RMASK|SDL_BUTTON_MMASK))return true;
    float rx,ry;
    if(!SDL_RenderCoordinatesFromWindow(renderer,event->wheel.mouse_x,event->wheel.mouse_y,&rx,&ry))return true;
    x=rx/hrp_iso_window_width_get();y=ry/hrp_iso_window_height_get();
    if(x<0 || x>1 || y<0 || y>1)return true;
    float delta=event->wheel.y;
    if(event->wheel.direction==SDL_MOUSEWHEEL_FLIPPED)delta=-delta;
    float minimum=fmaxf(.5f,fmaxf(zoom_base_width/4096.0f,zoom_base_height/4096.0f));
    float maximum=fminf(zoom_base_width/800.0f,zoom_base_height/600.0f);
    world_zoom=fmaxf(minimum,fminf(maximum,world_zoom*powf(1.12f,fmaxf(-8,fminf(8,delta)))));
    zoom_anchor_x=fmaxf(0,fminf(1,x));zoom_anchor_y=fmaxf(0,fminf(1,y));zoom_pending=true;
    return true;
}

void modern_ui_menu_tick(void)
{
    world_zoom_sync();
    if (!enabled) return;
    if(mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_SHOP) {
        in_game=true;panels_tick();
        if(pending_action>=0) {int action=pending_action;pending_action=-1;panels_action(action);}
        return;
    }
    int type=mainmenu_ui_is_active() ? (int)mainmenu_ui_window_type_get() : -1;
    if (type != menu_previous_type) {
        modern_graphics_cancel();
        if(name_focused)menu_name_focus(false);
        description_scroll=0;description_tab=2;derived_stats=false;
        sheet_category=1;sheet_page=sheet_selected=0;sheet_select_detail(1,0);message_text[0]=0;
        thumbnail_index=-2;
        if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME) {
            int selected=sub_5496C0(type)->selected_index;
            if(type==MM_WINDOW_SAVE_GAME) {
                if(selected<0)selected=0;
                modern_mainmenu_save_select(selected);
                if(selected==0)menu_name_focus(true);
            }
            save_page=selected>=0?selected/8:0;
        }
        options_tab=-1;key_edit=-1;
        if (type==MM_WINDOW_OPTIONS) modern_graphics_begin();
        menu_previous_type=type;
        button_count=0;
        pressed_action=-1;click_captured=false;
    }
    modern_graphics_tick();
    if (!native_menu() || pending_action<400 || pending_action>699) return;
    int action=pending_action;pending_action=-1;
    if(type==MM_WINDOW_OPTIONS && action>=430 && action<446) {
        if(options_tab>=0 && !modern_graphics.deadline)modern_options_adjust((action-430)/2,action%2?1:-1);
        return;
    }
    if(type==MM_WINDOW_OPTIONS && key_options_action(action))return;
    if(creation_menu_action(action))return;
    bool locked=type==MM_WINDOW_MAINMENU_IN_PLAY_LOCKED;
    menu_suspended=true; /* Classic confirmation dialogs must remain interactive. */
    switch (action) {
    case 400: modern_mainmenu_activate(locked?2:4); break;
    case 401: if(!locked) modern_mainmenu_activate(0); break;
    case 402: if(!locked) modern_mainmenu_activate(1); break;
    case 403: modern_mainmenu_activate(locked?0:2); break;
    case 404: modern_mainmenu_activate(locked?1:3); break;
    case 405: if(type==MM_WINDOW_OPTIONS)modern_options_return();else mainmenu_ui_close(true); break;
    case 406: if(!modern_graphics.deadline && modern_options_select(0))options_tab=0;break;
    case 407: if(!modern_graphics.deadline)options_tab=-1;break;
    case 408: if(!modern_graphics.deadline && modern_options_select(1))options_tab=1;break;
    case 409: if(!modern_graphics.deadline && modern_options_select(2))options_tab=2;break;
    case 410: modern_graphics_mode(-1); break;
    case 411: modern_graphics_mode(1); break;
    case 412: modern_graphics_resolution(-1); break;
    case 413: modern_graphics_resolution(1); break;
    case 414: modern_graphics_apply(); break;
    case 415: modern_graphics_confirm(); break;
    case 416: modern_graphics_cancel(); break;
    case 417:
        if(type==MM_WINDOW_OPTIONS && options_tab==0 && !tig_net_is_active()) {
            settings_set_value(&settings,MODERN_UNLIMITED_FOLLOWERS_KEY,settings_get_value(&settings,MODERN_UNLIMITED_FOLLOWERS_KEY)==1?0:1);
            settings_save(&settings);
        }
        break;
    }
    menu_suspended=false;
    button_count=0;
}

/* Native play panels share this file's text, materials and input helpers. */
#include "modern_panels.inc"
#include "modern_combat.inc"
#include "modern_party.inc"

/* Stable-size maintenance controls for the character currently controlled. */
static int maintained_ids[32],maintained_spells[32],maintained_count,maintained_page;
static int64_t maintained_actor,maintained_pressed_actor;
static int maintained_pressed_id=-1;
static SDL_FRect maintained_rect;

static bool maintained_region(float x,float y)
{
    return hud && !journal && !native_panel() && maintained_rect.w>0 && inside(maintained_rect,x,y);
}

static void draw_maintained_spells(void)
{
    maintained_rect=(SDL_FRect){0};
    if(!hud || journal || native_panel())return;
    int64_t actor=combat_input_actor();
    /* Keep the player's effects visible during enemy turns, without commands. */
    if(!actor)actor=player_get_local_pc_obj();
    if(!actor || !obj_handle_is_valid(actor))return;
    if(actor!=maintained_actor){maintained_page=0;maintained_actor=actor;}
    maintained_count=modern_maintained_spells(actor,maintained_ids,maintained_spells,32);
    int capacity=stat_level_get(actor,STAT_INTELLIGENCE)/4;
    if(capacity<0)capacity=0;if(capacity>5)capacity=5;
    int slots=maintained_count>capacity?maintained_count:capacity;
    if(!slots)return;
    int pages=(slots+4)/5;if(maintained_page>=pages)maintained_page=pages-1;
    int visible=slots-maintained_page*5;if(visible>5)visible=5;
    float width=170+visible*46+(pages>1?62:0),x=(canvas_w-width)/2,y=82;
    maintained_rect=(SDL_FRect){x,y,width,54};
    fill(maintained_rect,(SDL_Color){19,14,10,225});outline(maintained_rect,MUTED);
    text_compact(tactical_text(TC_MAINTAINED),x+10,y+7,150,false,GOLD);
    char count[40];snprintf(count,sizeof(count),"%d / %d",maintained_count,capacity);
    text_draw(0,count,x+10,y+30,MUTED);
    bool ready=combat_input_ready(actor) && intgame_mode_get()!=INTGAME_MODE_DIALOG;
    int hovered=-1;SDL_FRect hover={0};
    for(int i=0;i<visible;i++) {
        int index=maintained_page*5+i;bool active=index<maintained_count;
        SDL_FRect r={x+164+i*46,y+6,40,42};
        button(r,2500+i,"",-1,active,active && ready);
        if(active) {
            draw_art(spell_icon(maintained_spells[index]),(SDL_FRect){r.x+4,r.y+4,32,32},true);
            if(inside(r,mouse_x,mouse_y)){hovered=index;hover=r;}
        }
    }
    if(pages>1) {
        button((SDL_FRect){x+width-62,y+13,26,28},2505,"<",-1,false,maintained_page>0);
        button((SDL_FRect){x+width-32,y+13,26,28},2506,">",-1,false,maintained_page+1<pages);
    }
    if(hovered>=0) {
        char name[2048];SDL_strlcpy(name,game_text(spell_name(maintained_spells[hovered])),sizeof(name));
        float w=fminf(540,fmaxf(260,text_measure(1,name)+24)),xx=fmaxf(8,fminf(canvas_w-w-8,hover.x));
        SDL_FRect content={xx+12,y+66,w-24,0};int lines=menu_paragraph(name,content,0,GOLD);
        if(lines>3)lines=3;
        float h=lines*27+42;
        fill((SDL_FRect){xx,y+60,w,h},(SDL_Color){19,14,10,245});outline((SDL_FRect){xx,y+60,w,h},MUTED);
        content.h=lines*27;menu_paragraph(name,content,0,GOLD);
        text_compact(ready?tr("Cliquer pour arrêter le sort","Click to stop the spell"):tactical_text(actor==combat_input_actor()?TC_BUSY:TC_WAIT),xx+12,y+70+lines*27,w-24,false,MUTED);
    }
}

static bool maintained_action(int action)
{
    if(action<2500 || action>2506)return false;
    if(!hud || journal || native_panel() || mainmenu_ui_is_active())return true;
    if(action>=2505){maintained_page+=action==2505?-1:1;if(maintained_page<0)maintained_page=0;return true;}
    /* The slot can change while the mouse is held: act on the pressed run ID. */
    if(maintained_pressed_id>=0 && maintained_pressed_actor==combat_input_actor()
        && intgame_mode_get()!=INTGAME_MODE_DIALOG)
        modern_spell_stop(maintained_pressed_actor,maintained_pressed_id);
    maintained_pressed_id=-1;
    return true;
}

static void draw_quest_tracker(void)
{
    if(journal || native_panel() || tactical_active() || !hud)return;
    notebook_sync();if(tracked_quest<0)return;
    if(!tracked_refresh || SDL_GetTicks()-tracked_refresh>2000){
        tracked_refresh=SDL_GetTicks();tracked_text[0]=0;
        int64_t pc=player_get_local_pc_obj();QuestLogbookEntry quests[2000];
        int count=quest_get_logbook_data(pc,quests);
        for(int i=0;i<count;i++)if(quests[i].num==tracked_quest){
            if(quests[i].state==QUEST_STATE_ACCEPTED || quests[i].state==QUEST_STATE_ACHIEVED){
                char raw[32768];quest_copy_description(pc,tracked_quest,raw,sizeof(raw));
                from_game(raw,tracked_text,sizeof(tracked_text));
            }
            break;
        }
    }
    if(!*tracked_text)return;
    SDL_FRect r={canvas_w-335,85,320,100};
    fill(r,(SDL_Color){19,14,10,205});outline(r,MUTED);
    text_draw(0,tr("Quête suivie","Tracked quest"),r.x+10,r.y+7,GOLD);
    menu_paragraph(tracked_text,(SDL_FRect){r.x+10,r.y+29,r.w-20,64},0,GOLD);
    if(button_count<MAX_BUTTONS)buttons[button_count++]=(Button){r,2301,true};
}


static bool hud_region(float x,float y)
{
    if(maintained_region(x,y) || party_region(x,y))return true;
    if(!tactical_active() && tracked_quest>=0 && *tracked_text && inside((SDL_FRect){canvas_w-335,85,320,100},x,y))return true;
    if(tactical_active())return tactical_region(x,y);
    return inside((SDL_FRect){(canvas_w-930)/2,10,930,60},x,y)
        || inside((SDL_FRect){(canvas_w-980)/2,canvas_h-126,980,116},x,y);
}

static int hit(float x,float y)
{
    for(int i=button_count-1;i>=0;i--) if(buttons[i].enabled && inside(buttons[i].rect,x,y)) return buttons[i].action;
    return -1;
}

static void quickslot_hint(int slot,char* hint,size_t capacity)
{
    hint[0]=0;
    if(slot<0 || slot>=10)return;
    Hotkey* hk=sub_57F240(slot);
    if(!hk || hk->type==HOTKEY_NONE)return;
    char name[2048]="";
    if(hk->type==HOTKEY_ITEM || hk->type==HOTKEY_ITEM_SPELL) {
        int64_t item=hk->item_obj.obj;
        if(!item || !obj_handle_is_valid(item))return;
        /* Examine preserves identification rules and the installed translation. */
        object_examine(item,player_get_local_pc_obj(),name);
    } else if(hk->type==HOTKEY_SPELL && hk->data>=0 && hk->data<SPELL_COUNT)
        SDL_strlcpy(name,spell_name(hk->data),sizeof(name));
    else if(hk->type==HOTKEY_SKILL && hk->data>=0 && hk->data<4)
        SDL_strlcpy(name,sub_57A700(hk->data),sizeof(name));
    if(!*name)return;
    snprintf(hint,capacity,"[%d] %s",(slot+1)%10,game_text(name));
    if(hk->type==HOTKEY_ITEM_SPELL && hk->data>=0 && hk->data<SPELL_COUNT) {
        SDL_strlcat(hint," · ",capacity);
        SDL_strlcat(hint,game_text(spell_name(hk->data)),capacity);
    }
}

static void draw_quickslot_hint(void)
{
    SDL_FRect anchor={0};int slot=-1;
    if(native_panel()) {
        slot=binding_slot_at(mouse_x,mouse_y);
        if(slot>=0)anchor=binding_slots[slot];
    } else if(hud) {
        for(int i=button_count-1;i>=0;i--)if(inside(buttons[i].rect,mouse_x,mouse_y)) {
            if(buttons[i].action>=300 && buttons[i].action<310){slot=buttons[i].action-300;anchor=buttons[i].rect;}
            break;
        }
    }
    char hint[4096];quickslot_hint(slot,hint,sizeof(hint));if(!*hint)return;
    float width=fminf(fminf(540,canvas_w-24),fmaxf(150,text_measure(1,hint)+28));
    float xx=fmaxf(12,fminf(canvas_w-width-12,anchor.x+anchor.w/2-width/2));
    SDL_FRect content={xx+14,0,width-28,0};
    int lines=menu_paragraph(hint,content,0,GOLD);if(lines>4)lines=4;
    float height=lines*27+18,yy=fmaxf(8,anchor.y-height-8);
    SDL_FRect box={xx,yy,width,height};fill(box,(SDL_Color){19,14,10,245});outline(box,MUTED);
    content.y=yy+9;content.h=height-18;menu_paragraph(hint,content,0,GOLD);
}

static void render_overlay(SDL_Renderer* target)
{
    if(slide_ui_is_active())return;
    bool menu=native_menu();
    if(!menu && !native_panel() && (!visible() || (!hud && !journal))) return;
    renderer=target;
    if(journal && !capturing && !captured && SDL_getenv("ARCANUM_UI_CAPTURE_DIR")) capture_reviews();
    update_geometry();
    int ww,wh;
    SDL_GetWindowSize(window,&ww,&wh);
    float mx,my;SDL_GetMouseState(&mx,&my);
    if(ww>0 && wh>0) {mouse_x=mx*canvas_w/ww;mouse_y=my*canvas_h/wh;}
    int old_w,old_h;SDL_RendererLogicalPresentation old_mode;
    SDL_GetRenderLogicalPresentation(renderer,&old_w,&old_h,&old_mode);
    SDL_SetRenderLogicalPresentation(renderer,(int)canvas_w,(int)canvas_h,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    SDL_BlendMode old_blend;SDL_GetRenderDrawBlendMode(renderer,&old_blend);
    SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
    button_count=0;
    if(menu) draw_menu();
    else {
        draw_world_bubbles();
        if(!hud_region(mouse_x,mouse_y))draw_world_hover();
        draw_maintained_spells();
        if(hud){if(tactical_active()){draw_tactical_preview();draw_tactical_hud();}else draw_hud();}
        draw_party();
        draw_quest_tracker();
        if(journal) draw_journal();
        draw_play_panels();
        draw_binding_bar();
    }
    if(key_edit<0)for(int i=button_count-1;i>=0;i--)if(inside(buttons[i].rect,mouse_x,mouse_y)) {
        int action=buttons[i].action,binding=key_for_action(action);char hint[256]="";
        if(binding>=0)snprintf(hint,sizeof(hint),"%s [%s]",key_label(binding),key_name(binding));
        else if(action>=2000 && action<2128)SDL_strlcpy(hint,(action%2)?tr("Activer / arrêter le suivi de cette quête","Track / untrack this quest"):tr("Ajouter / retirer des favoris","Add / remove favorite"),sizeof(hint));
        if(*hint) {
            float width=fminf(450,text_measure(0,hint)+24),xx=fminf(canvas_w-width-8,fmaxf(8,mouse_x-width/2));
            float yy=buttons[i].rect.y>canvas_h/2?buttons[i].rect.y-34:buttons[i].rect.y+buttons[i].rect.h+6;
            fill((SDL_FRect){xx,yy,width,28},(SDL_Color){19,14,10,245});outline((SDL_FRect){xx,yy,width,28},MUTED);
            text_compact(hint,xx+12,yy+6,width-24,true,GOLD);
        }
        break;
    }
    if(!menu && !journal)draw_quickslot_hint();
    if(menu || journal || native_panel() || (hud && hud_region(mouse_x,mouse_y))) draw_art(0,(SDL_FRect){mouse_x,mouse_y,28,35},true);
    SDL_SetRenderDrawBlendMode(renderer,old_blend);
    SDL_SetRenderLogicalPresentation(renderer,old_w,old_h,old_mode);
}

static void search_focus(bool on)
{
    searching=on;
    if(on) SDL_StartTextInput(window); else SDL_StopTextInput(window);
}

static void save_settings(void)
{
    FILE* f=fopen("modern-ui/settings.ini.tmp","w");
    if(f) {
        fprintf(f,"TextSize=%d\n",body_size);
        if(fclose(f)==0) rename("modern-ui/settings.ini.tmp","modern-ui/settings.ini");
    }
}

static void activate(int action)
{
    if((action>=2000 && action<2128) || action==2300 || action==2301){notebook_action(action);return;}
    if(maintained_action(action))return;
    if(party_action(action))return;
    if(tactical_action(action))return;
    if(panels_action(action))return;
    /* A conversation may begin between a HUD click and its deferred dispatch. */
    if(intgame_mode_get()==INTGAME_MODE_DIALOG) return;
    if(action>=100 && action<107) {
        chapter=action-100;spread=0;search[0]=0;quest_filter=0;dirty=true;search_focus(false);
    } else switch(action) {
    case 200: logbook_ui_close();break;
    case 201: search_focus(true);break;
    case 202: if(body_size>16) {body_size-=2;dirty=true;spread=0;save_settings();} break;
    case 203: if(body_size<26) {body_size+=2;dirty=true;spread=0;save_settings();} break;
    case 204: if(spread>0) spread--;break;
    case 205: if(spread+1<spread_count) spread++;break;
    case 210:case 211:case 212:case 213:case 214:quest_filter=action-210;spread=0;dirty=true;break;
    default:
        if(action>=300 && action<310) {
            Hotkey* hk=sub_57F240(action-300);
            if(hk && hk->type!=HOTKEY_NONE) intgame_hotkey_activate(hk);
        } else modern_intgame_action(action);
    }
}

static bool handle_event(SDL_Event* event)
{
    static bool captured_keys[SDL_SCANCODE_COUNT];
    bool keyboard = event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP;
    SDL_Scancode key = keyboard ? event->key.scancode : SDL_SCANCODE_UNKNOWN;
    bool valid_key = key > SDL_SCANCODE_UNKNOWN && key < SDL_SCANCODE_COUNT;

    /* A key press belongs to the UI that consumed it, including its release.
     * Escape may close that UI before KEY_UP arrives; forwarding the release
     * would reopen the pause menu in the legacy game loop.
     */
    if (keyboard && valid_key && captured_keys[key]) {
        if (event->type == SDL_EVENT_KEY_UP) captured_keys[key] = false;
        return true;
    }
    bool consumed = handle_ui_event(event);
    if (consumed && event->type == SDL_EVENT_KEY_DOWN && valid_key) captured_keys[key] = true;
    return consumed;
}

static bool modern_camera_enabled(void)
{
    int mode=intgame_mode_get();
    return enabled && in_game && !mainmenu_ui_is_active() && !journal && !native_panel()
        && !slide_ui_is_active() && !searching && !menu_suspended
        && SDL_GetKeyboardFocus()==window && !SDL_TextInputActive(window)
        && !(SDL_GetModState()&(SDL_KMOD_CTRL|SDL_KMOD_ALT|SDL_KMOD_GUI))
        && (mode==INTGAME_MODE_MAIN || mode==INTGAME_MODE_SPELL || mode==INTGAME_MODE_SKILL || mode==INTGAME_MODE_ITEM);
}
bool modern_camera_key(int scancode)
{
    if(!modern_camera_enabled() || scancode<0 || scancode>=SDL_SCANCODE_COUNT)return false;
    const int directions[]={SDL_SCANCODE_W,SDL_SCANCODE_A,SDL_SCANCODE_S,SDL_SCANCODE_D};
    for(int i=0;i<4;i++)if(scancode==directions[i])return key_binding[KB_UP+i] && SDL_GetKeyboardState(NULL)[key_binding[KB_UP+i]];
    return false;
}

static bool handle_ui_event(SDL_Event* event)
{
    if(slide_ui_is_active())return false;
    bool menu=native_menu();
    if(in_game && fonts[0].texture && !mainmenu_ui_is_active()
        && (event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP)
        && event->key.scancode==SDL_SCANCODE_F9) {
        if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) pending_action=999;
        return true;
    }
    if(!menu && panels_event(event))return true;
    if(!menu && party_event(event))return true;
    if(world_zoom_event(event))return true;
    if(!menu && (!visible() || (!hud && !journal))) return false;
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    if(ww<=0 || wh<=0) return false;
    if(event->type==SDL_EVENT_MOUSE_MOTION) {
        mouse_x=event->motion.x*canvas_w/ww;mouse_y=event->motion.y*canvas_h/wh;
        return menu; /* Block hover/clicks on the covered classic menu. */
    }
    if(event->type==SDL_EVENT_MOUSE_BUTTON_DOWN || event->type==SDL_EVENT_MOUSE_BUTTON_UP) {
        mouse_x=event->button.x*canvas_w/ww;mouse_y=event->button.y*canvas_h/wh;
        bool over=menu || journal || (hud && hud_region(mouse_x,mouse_y));
        if(event->button.down && over) {
            tactical_pressed_actor=combat_input_actor();
            click_captured=true;pressed_action=event->button.button==SDL_BUTTON_LEFT?hit(mouse_x,mouse_y):-1;
            maintained_pressed_id=-1;
            if(pressed_action>=2500 && pressed_action<2505) {
                int index=maintained_page*5+pressed_action-2500;
                if(index<maintained_count){maintained_pressed_id=maintained_ids[index];maintained_pressed_actor=maintained_actor;}
            }
            return true;
        }
        if(!event->button.down && click_captured) {
            if(event->button.button==SDL_BUTTON_LEFT && pressed_action==hit(mouse_x,mouse_y)) pending_action=pressed_action;
            pressed_action=-1;click_captured=false;return true;
        }
        return menu || journal;
    }
    if(menu) {
        MainMenuWindowType type=mainmenu_ui_window_type_get();
        if(type==MM_WINDOW_OPTIONS && key_options_event(event))return true;
        if(!confirming_menu && event->type==SDL_EVENT_TEXT_INPUT && name_focused) {
            menu_name_input(event->text.text,false);return true;
        }
        if(!confirming_menu && event->type==SDL_EVENT_MOUSE_WHEEL) {
            if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME)pending_action=event->wheel.y>0?560:561;
            else pending_action=event->wheel.y>0?540:541;
            return true;
        }
        if(!confirming_menu && name_focused && event->type==SDL_EVENT_KEY_DOWN) {
            if(event->key.scancode==SDL_SCANCODE_ESCAPE || event->key.scancode==SDL_SCANCODE_RETURN || event->key.scancode==SDL_SCANCODE_TAB) {
                menu_name_focus(false);
                if(type==MM_WINDOW_SAVE_GAME && event->key.scancode==SDL_SCANCODE_RETURN && !event->key.repeat)pending_action=500;
            }
            else if(event->key.scancode==SDL_SCANCODE_BACKSPACE)menu_name_input("",true);
            else if(event->key.scancode==SDL_SCANCODE_A && (event->key.mod&SDL_KMOD_CTRL))name_replace=true;
            return true;
        }
        if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
            if(event->type==SDL_EVENT_KEY_DOWN && !event->key.repeat) {
                MainMenuWindowType type=mainmenu_ui_window_type_get();
                if(confirming_menu) {
                    if(event->key.scancode==SDL_SCANCODE_ESCAPE || event->key.scancode==SDL_SCANCODE_N) pending_action=451;
                    if(event->key.scancode==SDL_SCANCODE_RETURN || event->key.scancode==SDL_SCANCODE_Y) pending_action=450;
                } else if(event->key.scancode==SDL_SCANCODE_ESCAPE) {
                    if(modern_graphics.deadline) pending_action=416;
                    else if(type==MM_WINDOW_MAINMENU_IN_PLAY || type==MM_WINDOW_MAINMENU_IN_PLAY_LOCKED || type==MM_WINDOW_26)
                        pending_action=500+sub_5496C0(type)->num_buttons-1;
                    else if(type!=MM_WINDOW_MAINMENU) pending_action=405;
                } else if(event->key.scancode==SDL_SCANCODE_RETURN && modern_graphics.deadline) pending_action=415;
                else if(type==MM_WINDOW_LOAD_GAME || type==MM_WINDOW_SAVE_GAME) {
                    int selected=sub_5496C0(type)->selected_index;
                    if(event->key.scancode==SDL_SCANCODE_UP && selected>0) {
                        modern_mainmenu_save_select(selected-1);save_page=(selected-1)/8;thumbnail_index=-2;
                    } else if(event->key.scancode==SDL_SCANCODE_DOWN && selected+1<sub_5496C0(type)->cnt) {
                        modern_mainmenu_save_select(selected+1);save_page=(selected+1)/8;thumbnail_index=-2;
                    } else if(event->key.scancode==SDL_SCANCODE_RETURN
                        && (type==MM_WINDOW_SAVE_GAME ? selected>=0 : modern_mainmenu_save_info() && modern_mainmenu_save_info()->version==25))pending_action=500;
                    else if(event->key.scancode==SDL_SCANCODE_PAGEUP)pending_action=560;
                    else if(event->key.scancode==SDL_SCANCODE_PAGEDOWN)pending_action=561;
                } else if(type==MM_WINDOW_PREGEN_CHAR) {
                    if(event->key.scancode==SDL_SCANCODE_LEFT)pending_action=502;
                    if(event->key.scancode==SDL_SCANCODE_RIGHT)pending_action=503;
                    if(event->key.scancode==SDL_SCANCODE_RETURN)pending_action=500;
                } else if(type==MM_WINDOW_NEW_CHAR && event->key.scancode==SDL_SCANCODE_TAB)pending_action=522;
                else if(event->key.scancode==SDL_SCANCODE_PAGEUP)pending_action=540;
                else if(event->key.scancode==SDL_SCANCODE_PAGEDOWN)pending_action=541;
            }
            return true;
        }
        return event->type==SDL_EVENT_MOUSE_WHEEL || event->type==SDL_EVENT_TEXT_INPUT;
    }
    if(event->type==SDL_EVENT_MOUSE_WHEEL && journal) {
        if(event->wheel.y<0 && spread+1<spread_count) spread++;
        if(event->wheel.y>0 && spread>0) spread--;
        return true;
    }
    if(event->type==SDL_EVENT_TEXT_INPUT && searching) {
        if(strlen(search)+strlen(event->text.text)<sizeof(search)) SDL_strlcat(search,event->text.text,sizeof(search));
        dirty=true;spread=0;return true;
    }
    if(event->type==SDL_EVENT_KEY_DOWN || event->type==SDL_EVENT_KEY_UP) {
        bool down=event->type==SDL_EVENT_KEY_DOWN;
        SDL_Scancode key=event->key.scancode;
        if(journal) {
            if(down && !event->key.repeat) {
                if(key==SDL_SCANCODE_ESCAPE) {
                    if(searching) search_focus(false);else pending_action=200;
                } else if(searching && key==SDL_SCANCODE_BACKSPACE) {
                    size_t n=strlen(search);
                    if(n) {do {n--;} while(n>0 && ((unsigned char)search[n]&0xC0)==0x80);search[n]=0;dirty=true;spread=0;}
                } else if(searching && key==SDL_SCANCODE_RETURN) search_focus(false);
                else if(!searching) {
                    if(key_is(KB_JOURNAL,key)) pending_action=200;
                    if(key==SDL_SCANCODE_RIGHT || key==SDL_SCANCODE_PAGEDOWN) pending_action=205;
                    if(key==SDL_SCANCODE_LEFT || key==SDL_SCANCODE_PAGEUP) pending_action=204;
                }
            }
            return true;
        }
        if(!(event->key.mod&(SDL_KMOD_CTRL|SDL_KMOD_ALT|SDL_KMOD_GUI)) && !SDL_TextInputActive(window)) {
            for(int i=KB_UP;i<KB_COUNT;i++)if(key_is(i,key))return modern_camera_enabled();
            const int actions[]={2,0,1,3,6,5,8,9,951,950,1100};
            for(int i=0;i<KB_UP;i++)if(key_is(i,key)) {
                if(down && !event->key.repeat && !native_panel() && intgame_mode_get()!=INTGAME_MODE_DIALOG) {
                    tactical_pressed_actor=combat_input_actor();pending_action=actions[i];
                    if(tactical_active() && i==KB_INVENTORY)pending_action=1104;
                    if(tactical_active() && i==KB_CHARACTER)pending_action=1105;
                    if(i==KB_END && !tactical_active())pending_action=-1;
                }
                return true;
            }
            /* Reassigned keys must not fall through to their old engine action. */
            for(int i=0;i<KB_UP;i++)if(key_defaults[i]==key)return true;
        }
    }
    return false;
}

static SDL_Texture* make_material(bool paper)
{
    Uint32 pixels[256*256];
    unsigned noise=0x5A37;
    for(int y=0;y<256;y++) for(int x=0;x<256;x++) {
        noise=noise*1664525u+1013904223u;
        int n=(int)((noise>>24)&15)-7;
        int grain=(int)(7*sin(y*.7+sin(x*.04)*2)+4*sin(y*.13+x*.01));
        int r=paper?217+n:49+n/2+grain;
        int g=paper?202+n:31+n/3+grain/2;
        int b=paper?168+n:21+n/4+grain/3;
        pixels[y*256+x]=0xFF000000u|((Uint32)r<<16)|((Uint32)g<<8)|(Uint32)b;
    }
    return rgba_texture(256,256,pixels);
}

bool modern_ui_init(void)
{
    if(SDL_getenv("ARCANUM_LEGACY_UI")) return false;
    if(tig_video_renderer_get(&renderer)!=TIG_OK || tig_video_window_get(&window)!=TIG_OK) return false;
    book_font_data=SDL_LoadFile("modern-ui/Book.ttf",&book_font_size);
    heading_font_data=SDL_LoadFile("modern-ui/Heading.ttf",&heading_font_size);
    if(!book_font_data || !heading_font_data) {modern_ui_exit();return false;}
    FILE* settings=fopen("modern-ui/settings.ini","r");
    if(settings) {int n;if(fscanf(settings,"TextSize=%d",&n)==1 && n>=16 && n<=26) body_size=n;fclose(settings);}
    key_load();
    update_geometry();
    if(!fonts[0].texture || !fonts[1].texture || !fonts[2].texture || !fonts[3].texture) {modern_ui_exit();return false;}
    wood_texture=make_material(false);paper_texture=make_material(true);
    SDL_Surface* background=SDL_LoadPNG("modern-ui/menu-background.png");
    if(background) {
        menu_background_width=(float)background->w;menu_background_height=(float)background->h;
        menu_background=SDL_CreateTextureFromSurface(renderer,background);
        SDL_DestroySurface(background);
    }
    SDL_Surface* inventory_actions=SDL_LoadPNG("modern-ui/inventory-actions.png");
    if(inventory_actions) {
        inventory_actions_texture=SDL_CreateTextureFromSurface(renderer,inventory_actions);
        SDL_DestroySurface(inventory_actions);
        if(inventory_actions_texture)SDL_SetTextureScaleMode(inventory_actions_texture,SDL_SCALEMODE_LINEAR);
    }
    SDL_Surface* controls=SDL_LoadPNG("modern-ui/control-icons.png");
    if(controls) {
        control_icons_texture=SDL_CreateTextureFromSurface(renderer,controls);
        SDL_DestroySurface(controls);
        if(control_icons_texture) {
            SDL_SetTextureScaleMode(control_icons_texture,SDL_SCALEMODE_LINEAR);
            SDL_SetTextureBlendMode(control_icons_texture,SDL_BLENDMODE_BLEND);
        }
    }
    SDL_Surface* skill_icons=SDL_LoadPNG("modern-ui/skill-icons.png");
    if(skill_icons) {
        skill_icons_texture=SDL_CreateTextureFromSurface(renderer,skill_icons);
        SDL_DestroySurface(skill_icons);
        if(skill_icons_texture)SDL_SetTextureScaleMode(skill_icons_texture,SDL_SCALEMODE_LINEAR);
    }
    mes_load("mes\\OptionsOffOn.mes",&option_boolean_file);
    enabled=true;
    combat_tactical_enable();
    if(SDL_getenv("ARCANUM_UI_SELFTEST")) verify_text_layout();
    tig_video_set_overlay(render_overlay);
    tig_message_set_native_filter(handle_event);
    tig_debug_printf("Arcanum Modern: native interface initialized.\n");
    return true;
}

bool modern_ui_enabled(void) {return enabled;}

void modern_ui_tick(void)
{
    modern_ui_menu_tick();
    tactical_tick();
    if(pending_action==999 && tactical_active())pending_action=-1;
    if(pending_action==999) {
        pending_action=-1;
        if(journal) logbook_ui_close();
        enabled=!enabled;
        modern_map_action(8);
        if(!enabled) {modern_intgame_hud_hide(false);hud=false;}
    }
    world_zoom_sync();
    modern_sleep_sync(enabled);
    modern_fate_sync(enabled);
    modern_bubbles_sync(enabled);
    modern_written_sync(enabled);
    if(!enabled) {modern_followers_sync(false);return;}
    in_game=true;
    panels_tick();
    if(pending_action>=0) {int action=pending_action;pending_action=-1;activate(action);}
    bool active=visible();
    hud=active && !intgame_is_compact_interface()
        /* Dialogue choices live in the world window, independently of the bars.
         * Keep our HUD when an NPC starts talking (including the opening scene).
         */
        && (intgame_mode_get()==INTGAME_MODE_MAIN || intgame_mode_get()==INTGAME_MODE_DIALOG || intgame_mode_get()==INTGAME_MODE_SPELL || intgame_mode_get()==INTGAME_MODE_SKILL || intgame_mode_get()==INTGAME_MODE_ITEM || journal)
        && iso_interface_window_get()==ROTWIN_TYPE_MSG;
    modern_intgame_hud_hide(hud || journal || native_panel());
    party_tick();
}

void modern_ui_journal_open(void)
{
    journal=true;spread=0;dirty=true;search[0]=0;quest_filter=0;
    tig_mouse_set_button(TIG_MOUSE_BUTTON_LEFT,false);
}

void modern_ui_journal_close(void)
{
    journal=false;search_focus(false);dirty=true;
}

void modern_ui_message(const char* text)
{
    if(!enabled || !text || !*text) return;
    from_game(text,message_text,sizeof(message_text));message_time=SDL_GetTicks();
    for(char* p=message_text;*p;p++) if(*p=='\n' || *p=='\r') *p=' ';
}

void modern_ui_exit(void)
{
    tactical_portraits_clear();
    if(option_boolean_file!=MES_FILE_HANDLE_INVALID){mes_unload(option_boolean_file);option_boolean_file=MES_FILE_HANDLE_INVALID;}
    modern_graphics_cancel();
    tig_video_set_overlay(NULL);tig_message_set_native_filter(NULL);
    for(int i=0;i<FONT_COUNT;i++) {SDL_DestroyTexture(fonts[i].texture);fonts[i].texture=NULL;}
    for(int i=0;i<art_count;i++) SDL_DestroyTexture(arts[i].texture);
    art_count=0;
    SDL_DestroyTexture(wood_texture);SDL_DestroyTexture(paper_texture);
    SDL_DestroyTexture(menu_background);menu_background=NULL;
    SDL_DestroyTexture(skill_icons_texture);skill_icons_texture=NULL;
    SDL_DestroyTexture(control_icons_texture);control_icons_texture=NULL;
    SDL_DestroyTexture(map_texture);map_texture=NULL;
    SDL_DestroyTexture(inventory_actions_texture);inventory_actions_texture=NULL;
    SDL_DestroyTexture(portrait_texture);portrait_texture=NULL;portrait_number=-1;
    SDL_DestroyTexture(save_texture);save_texture=NULL;thumbnail_index=-2;
    wood_texture=paper_texture=NULL;
    SDL_free(book_font_data);SDL_free(heading_font_data);
    book_font_data=heading_font_data=NULL;
    free(lines);lines=NULL;line_count=line_capacity=0;
    enabled=false;
}

/* Optional QA captures exercise the real journal at native display resolutions. */
static void capture_reviews(void)
{
    const char* directory=SDL_getenv("ARCANUM_UI_CAPTURE_DIR");
    SDL_Surface* background=SDL_RenderReadPixels(renderer,NULL);
    if(!background) {captured=true;return;}
    SDL_Texture* bg=SDL_CreateTextureFromSurface(renderer,background);
    SDL_DestroySurface(background);
    if(!bg) {captured=true;return;}
    SDL_Texture* previous=SDL_GetRenderTarget(renderer);
    int ow,oh;SDL_RendererLogicalPresentation mode;
    SDL_GetRenderLogicalPresentation(renderer,&ow,&oh,&mode);
    const int sizes[][2]={{1920,1080},{2560,1440},{3840,2160},{1280,1024}};
    capturing=true;
    for(int i=0;i<4;i++) {
        int w=sizes[i][0],h=sizes[i][1];
        SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,w,h);
        if(!target) continue;
        SDL_SetRenderTarget(renderer,target);
        SDL_SetRenderLogicalPresentation(renderer,w,h,SDL_LOGICAL_PRESENTATION_DISABLED);
        SDL_RenderTexture(renderer,bg,NULL,NULL);
        render_overlay(renderer);
        SDL_Surface* result=SDL_RenderReadPixels(renderer,NULL);
        if(result) {
            char path[1024];snprintf(path,sizeof(path),"%s/journal-%dx%d.bmp",directory,w,h);
            SDL_SaveBMP(result,path);SDL_DestroySurface(result);
            tig_debug_printf("Modern UI capture: %dx%d, %d journal lines, %d spreads\n",w,h,(int)line_count,spread_count);
        }
        SDL_SetRenderTarget(renderer,previous);
        SDL_DestroyTexture(target);
    }
    SDL_SetRenderLogicalPresentation(renderer,ow,oh,mode);
    SDL_DestroyTexture(bg);
    capturing=false;captured=true;dirty=true;
}

/* Verify encoding, accent-insensitive search and lossless wrapping at each width. */
static void verify_text_layout(void)
{
    char converted[128],normalized[128];
    from_game("Une qu\xEAte, une cl\xE9.",converted,sizeof(converted));
    fold(converted,normalized,sizeof(normalized));
    if(strcmp(normalized,"une quete, une cle.")!=0) {
        tig_debug_printf("MODERN UI TEST FAILED: encoding/search\n");exit(EXIT_FAILURE);
    }
    char sample[12000],original[12000],rejoined[12000];
    SDL_strlcpy(sample,"1 janvier 1885\n",sizeof(sample));
    for(int i=0;i<60;i++) SDL_strlcat(sample,"Une clé oubliée mène à une quête. ",sizeof(sample));
    for(int i=0;i<400;i++) SDL_strlcat(sample,"w",sizeof(sample));
    SDL_strlcat(sample," fin.",sizeof(sample));
    size_t k=0;
    for(const char* p=sample;*p;p++) if(*p!=' ' && *p!='\n' && *p!='\r') original[k++]=*p;
    original[k]=0;
    float saved_width=text_width;
    const float widths[]={180,260,360,480};
    for(int w=0;w<4;w++) {
        text_width=widths[w];line_count=0;wrap_text(sample,0);k=0;
        for(size_t i=0;i<line_count;i++) {
            if(text_measure(lines[i].heading?0:1,lines[i].text)>text_width+.01f) {
                tig_debug_printf("MODERN UI TEST FAILED: line overflow\n");exit(EXIT_FAILURE);
            }
            for(const char* p=lines[i].text;*p;p++) if(*p!=' ') rejoined[k++]=*p;
        }
        rejoined[k]=0;
        if(strcmp(original,rejoined)!=0) {
            tig_debug_printf("MODERN UI TEST FAILED: text lost during wrapping\n");exit(EXIT_FAILURE);
        }
    }
    text_width=saved_width;line_count=0;dirty=true;
    tig_debug_printf("MODERN UI TEST PASS: CP1252 accents, search normalization, lossless wrapping at 4 widths\n");
}
