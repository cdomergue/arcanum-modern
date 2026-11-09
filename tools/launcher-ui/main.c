/* Native, portable launcher presentation. Update logic lives in tools/launcher.
 * State is private UTF-8 text; stdout carries only the four fixed UI actions. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SDL_Renderer* renderer;
static SDL_Texture *background, *font;
static stbtt_bakedchar glyphs[224];
static char *state, *status_text, *version_text, *heading, *play_text, *auto_text, *notes;
static int percent, automatic, playable, closable = 1, offer, scroll_line, focus_item;
static int feedback_tab, community_available, community_busy, french;
static char feedback_text[8001];
static SDL_FRect news_tab={82,303,130,30}, feedback_tab_button={222,303,140,30}, feedback_field={82,343,736,102}, send_button={608,489,210,26}, privacy_button={82,489,150,26}, github_button={244,489,350,26};
static SDL_FRect play_button = {562,525,256,42}, auto_button = {82,530,430,32}, language_button = {735,33,82,28}, install_button = {82,525,210,42};

static unsigned codepoint(const char** p)
{
    const unsigned char* s=(const unsigned char*)*p;
    unsigned c=*s++;
    if(c>=0xc0 && c<0xe0 && s[0]){c=((c&31)<<6)|(s[0]&63);s++;}
    else if(c>=0xe0 && c<0xf0 && s[0] && s[1]){c=((c&15)<<12)|((s[0]&63)<<6)|(s[1]&63);s+=2;}
    else if(c>=0xf0 && s[0] && s[1] && s[2]){c='?';s+=3;}
    *p=(const char*)s;
    if(c==0x2022)c='-';if(c==0x2019)c='\'';if(c==0x2013||c==0x2014)c='-';if(c==0x2026)c='.';
    return c>=32 && c<256?c:'?';
}
static float width(const char* text)
{float w=0;while(*text && *text!='\n'){unsigned c=codepoint(&text);w+=glyphs[c-32].xadvance;}return w;}
static void text(const char* str,float x,float y,SDL_Color color)
{
    SDL_SetTextureColorMod(font,color.r,color.g,color.b);SDL_SetTextureAlphaMod(font,color.a);
    while(*str && *str!='\n'){
        unsigned c=codepoint(&str);stbtt_aligned_quad q;float baseline=y+18;
        stbtt_GetBakedQuad(glyphs,512,512,(int)c-32,&x,&baseline,&q,1);
        SDL_FRect src={q.s0*512,q.t0*512,(q.s1-q.s0)*512,(q.t1-q.t0)*512};
        SDL_FRect dst={q.x0,q.y0,q.x1-q.x0,q.y1-q.y0};SDL_RenderTexture(renderer,font,&src,&dst);
    }
}
static void fill(SDL_FRect r,SDL_Color c){SDL_SetRenderDrawColor(renderer,c.r,c.g,c.b,c.a);SDL_RenderFillRect(renderer,&r);}
static void button(SDL_FRect r,const char* label,int focused,int enabled)
{
    fill(r,(SDL_Color){44,34,21,255});SDL_SetRenderDrawColor(renderer,156,130,92,255);SDL_RenderRect(renderer,&r);
    text(label,r.x+(r.w-width(label))/2,r.y+(r.h-22)/2,enabled?(SDL_Color){241,206,159,255}:(SDL_Color){115,112,103,255});
    if(focused){r.x+=4;r.y+=4;r.w-=8;r.h-=8;SDL_RenderRect(renderer,&r);}
}
static void action(const char* name){puts(name);fflush(stdout);}
static int inside(SDL_FRect r,float x,float y){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;}
static void read_state(const char* path)
{
    size_t length=0;char* data=SDL_LoadFile(path,&length);if(!data||length>4*1024*1024){SDL_free(data);return;}
    if(state && strcmp(state,data)==0){SDL_free(data);return;}
    /* Keep an untouched copy for change detection; pointers refer into a second copy. */
    static char* parsed;
    SDL_free(state);state=data;SDL_free(parsed);parsed=SDL_strdup(data);if(!parsed)return;
    char* fields[7];char* cursor=parsed;
    for(int i=0;i<6;i++){fields[i]=cursor;char* newline=strchr(cursor,'\n');if(!newline)return;*newline=0;cursor=newline+1;}fields[6]=cursor;
    char a[8],p[8],c[8];int old_offer=offer;offer=0;if(sscanf(fields[0],"%d %7s %7s %7s %d %d %d %d %d",&percent,a,p,c,&offer,&feedback_tab,&community_available,&community_busy,&french)<4)return;
    if(old_offer!=offer){scroll_line=0;focus_item=0;}
    automatic=strcmp(a,"true")==0;playable=strcmp(p,"true")==0;closable=strcmp(c,"true")==0;
    status_text=fields[1];version_text=fields[2];heading=fields[3];play_text=fields[4];auto_text=fields[5];notes=fields[6];
}
static SDL_FRect releases_button={82,489,736,26};
static void draw_notes(void)
{
    SDL_Rect clip={82,343,736,138};fill((SDL_FRect){82,343,736,138},(SDL_Color){23,17,12,255});SDL_SetRenderClipRect(renderer,&clip);
    const char* p=notes?notes:"";float x=92;int line=0;SDL_Color color={220,201,176,255};
    while(*p){
        if(*p=='\n'){p++;x=92;line++;continue;}
        if(x==92 || p==notes || p[-1]==' '){const char* end=p;float word=0;while(*end && *end!=' ' && *end!='\n'){unsigned c=codepoint(&end);word+=glyphs[c-32].xadvance;}if(x>92 && x+word>792){x=92;line++;}}
        const char* start=p;unsigned c=codepoint(&p);float advance=glyphs[c-32].xadvance;
        if(x+advance>792){x=92;line++;}
        if(line>=scroll_line && line<scroll_line+8){char letter[8]={0};size_t len=(size_t)(p-start);if(len<sizeof(letter)){memcpy(letter,start,len);text(letter,x,347+(line-scroll_line)*23,color);}}
        x+=advance;
    }
    SDL_SetRenderClipRect(renderer,NULL);int maximum=line-4;if(maximum<0)maximum=0;if(scroll_line>maximum)scroll_line=maximum;if(scroll_line<0)scroll_line=0;
    if(maximum){fill((SDL_FRect){809,349,3,124},(SDL_Color){63,51,33,255});float thumb=124*5.0f/(line+1);if(thumb<12)thumb=12;fill((SDL_FRect){809,349+(124-thumb)*scroll_line/maximum,3,thumb},(SDL_Color){170,139,91,255});}
}
static void draw(void)
{
    SDL_SetRenderDrawColor(renderer,0,0,0,255);SDL_RenderClear(renderer);SDL_RenderTexture(renderer,background,NULL,NULL);
    SDL_Color gold={241,206,159,255},muted={158,153,131,255};
    if(status_text)text(status_text,450-width(status_text)/2,242,gold);
    fill((SDL_FRect){80,282,740,5},(SDL_Color){60,48,38,255});if(percent>0)fill((SDL_FRect){80,282,740*(percent>100?100:percent)/100.0f,5},(SDL_Color){193,153,97,255});
    button(news_tab,french?"Actualités":"News",focus_item==3,1);button(feedback_tab_button,"Feedbacks",focus_item==4,1);
    if(!feedback_tab){SDL_Rect heading_clip={382,303,436,30};SDL_SetRenderClipRect(renderer,&heading_clip);if(heading)text(heading,382,308,muted);SDL_SetRenderClipRect(renderer,NULL);draw_notes();button(releases_button,french?"Versions plus anciennes sur GitHub":"Older releases on GitHub",focus_item==5,1);}else {
        fill(feedback_field,(SDL_Color){23,17,12,255});SDL_Rect clip={82,343,736,102};SDL_SetRenderClipRect(renderer,&clip);
        /* End-of-text editor: keep the last four wrapped lines visible. */
        int lines=0;float measure=92;for(const char* q=feedback_text;*q;){if(*q=='\n'){q++;measure=92;lines++;continue;}unsigned c=codepoint(&q);if(measure+glyphs[c-32].xadvance>800){measure=92;lines++;}measure+=glyphs[c-32].xadvance;}
        float x=92,y=347-(lines>3?(lines-3)*23:0);for(const char* q=feedback_text;*q;){if(*q=='\n'){q++;x=92;y+=23;continue;}const char* previous=q;unsigned c=codepoint(&q);if(x+glyphs[c-32].xadvance>800){x=92;y+=23;}char letter[8]={0};memcpy(letter,previous,(size_t)(q-previous));text(letter,x,y,gold);x+=glyphs[c-32].xadvance;}
        if(focus_item==5)fill((SDL_FRect){x,y+2,1,20},gold);
        SDL_SetRenderClipRect(renderer,NULL);clip=(SDL_Rect){82,449,736,38};SDL_SetRenderClipRect(renderer,&clip);if(notes)text(notes,82,449,muted);SDL_SetRenderClipRect(renderer,NULL);
        button(send_button,french?"Envoyer":"Send",focus_item==6,community_available&&!community_busy);
        button(privacy_button,french?"Confidentialité":"Privacy",focus_item==7,1);
        button(github_button,french?"Ça vous plaît ? Une étoile sur GitHub !":"You like it? Star it on GitHub!",focus_item==8,1);
    }
    button(play_button,play_text?play_text:"Play",focus_item==0,playable);button(language_button,"FR / EN",focus_item==2,1);
    if(offer)button(install_button,auto_text?auto_text:"Install",focus_item==1,1);else {
    fill((SDL_FRect){82,540,14,14},(SDL_Color){50,41,28,255});SDL_SetRenderDrawColor(renderer,193,153,97,255);SDL_FRect box={82,540,14,14};SDL_RenderRect(renderer,&box);if(automatic)text("x",84,535,gold);
    if(auto_text)text(auto_text,104,535,focus_item==1?gold:muted);}if(version_text)text(version_text,80,578,muted);
    SDL_RenderPresent(renderer);
}
static void community_action(int item,const char* path,SDL_Window* window)
{
    if(item==3){action("news");return;}if(item==4){action("feedback");return;}
    if(!feedback_tab){if(item==5)action("releases");return;}
    if(item==8){action("github");return;}
    if(item==6 && community_available && !community_busy){fputs("feedback:",stdout);for(const unsigned char* p=(unsigned char*)feedback_text;*p;p++)printf("%02x",*p);puts("");fflush(stdout);return;}
    if(item==7){
        char notice_path[4096];snprintf(notice_path,sizeof(notice_path),"%s.privacy",path);char* notice=SDL_LoadFile(notice_path,NULL);if(!notice)return;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION,french?"Confidentialité":"Privacy",notice,window);
        SDL_free(notice);
    }
}
int main(int argc,char** argv)
{
    bool check = argc==5 && strcmp(argv[4],"--check-render")==0;
    if(argc!=4 && !check)return 2;
    if(!SDL_Init(SDL_INIT_VIDEO)){fprintf(stderr,"%s\n",SDL_GetError());return 1;}
    SDL_Rect usable={0,0,1000,760};SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(),&usable);
    int width_px=900,height_px=620;if(usable.h<700){height_px=usable.h-60;width_px=height_px*900/620;}
    SDL_Window* window=SDL_CreateWindow("Arcanum Renaissance",width_px,height_px,SDL_WINDOW_HIGH_PIXEL_DENSITY|SDL_WINDOW_RESIZABLE|(check?SDL_WINDOW_HIDDEN:0));
    if(!window)return 1;renderer=SDL_CreateRenderer(window,NULL);if(!renderer)return 1;
    SDL_SetRenderLogicalPresentation(renderer,900,620,SDL_LOGICAL_PRESENTATION_LETTERBOX);
    int w,h,channels;unsigned char* image=stbi_load(argv[2],&w,&h,&channels,4);if(!image)return 1;
    SDL_Surface* surface=SDL_CreateSurfaceFrom(w,h,SDL_PIXELFORMAT_RGBA32,image,w*4);background=SDL_CreateTextureFromSurface(renderer,surface);SDL_DestroySurface(surface);stbi_image_free(image);
    size_t size;void* font_data=SDL_LoadFile(argv[3],&size);if(!font_data)return 1;
    unsigned char* bitmap=calloc(512*512,1);unsigned char* rgba=malloc(512*512*4);if(!bitmap||!rgba)return 1;
    if(stbtt_BakeFontBitmap(font_data,0,19,bitmap,512,512,32,224,glyphs)<=0)return 1;
    for(int i=0;i<512*512;i++){rgba[i*4]=rgba[i*4+1]=rgba[i*4+2]=255;rgba[i*4+3]=bitmap[i];}
    surface=SDL_CreateSurfaceFrom(512,512,SDL_PIXELFORMAT_RGBA32,rgba,512*4);font=SDL_CreateTextureFromSurface(renderer,surface);SDL_DestroySurface(surface);free(bitmap);free(rgba);SDL_free(font_data);SDL_SetTextureBlendMode(font,SDL_BLENDMODE_BLEND);
    SDL_StartTextInput(window);
    bool running=true;while(running){read_state(argv[1]);SDL_Event event;
        while(SDL_PollEvent(&event)){SDL_ConvertEventToRenderCoordinates(renderer,&event);
            if(event.type==SDL_EVENT_QUIT){if(closable){action("quit");running=false;}}
            else if(event.type==SDL_EVENT_MOUSE_WHEEL){scroll_line-=(int)(event.wheel.y*3);if(scroll_line<0)scroll_line=0;}
            else if(event.type==SDL_EVENT_TEXT_INPUT && feedback_tab && focus_item==5){size_t n=strlen(feedback_text),add=strlen(event.text.text);if(n+add<sizeof(feedback_text))memcpy(feedback_text+n,event.text.text,add+1);}
            else if(event.type==SDL_EVENT_MOUSE_BUTTON_UP&&event.button.button==SDL_BUTTON_LEFT){float x=event.button.x,y=event.button.y;if(inside(play_button,x,y)&&playable)action("play");else if(inside(offer?install_button:auto_button,x,y))action("auto");else if(inside(language_button,x,y))action("language");else {int item=-1;if(inside(news_tab,x,y))item=3;else if(inside(feedback_tab_button,x,y))item=4;else if(!feedback_tab&&inside(releases_button,x,y))item=5;else if(feedback_tab){if(inside(feedback_field,x,y))item=5;else if(inside(send_button,x,y))item=6;else if(inside(privacy_button,x,y))item=7;else if(inside(github_button,x,y))item=8;}if(item>=0){focus_item=item;community_action(item,argv[1],window);}}}
            else if(event.type==SDL_EVENT_KEY_DOWN){
                if(feedback_tab&&focus_item==5){size_t n=strlen(feedback_text);if(event.key.key==SDLK_V&&(event.key.mod&(SDL_KMOD_CTRL|SDL_KMOD_GUI))){char* pasted=SDL_GetClipboardText();if(pasted){size_t add=strlen(pasted);if(n+add<sizeof(feedback_text))memcpy(feedback_text+n,pasted,add+1);SDL_free(pasted);}continue;}if(event.key.key==SDLK_BACKSPACE&&n){do{n--;}while(n&&((unsigned char)feedback_text[n]&0xc0)==0x80);feedback_text[n]=0;continue;}if(event.key.key==SDLK_RETURN&&n+1<sizeof(feedback_text)){feedback_text[n]='\n';feedback_text[n+1]=0;continue;}if(event.key.key==SDLK_SPACE)continue;}
                switch(event.key.key){case SDLK_ESCAPE:if(closable){action("quit");running=false;}break;case SDLK_TAB:focus_item=(focus_item+1)%(feedback_tab?9:6);break;case SDLK_RETURN:case SDLK_SPACE:if(focus_item==0){if(playable)action("play");}else if(focus_item<3)action(focus_item==1?"auto":"language");else community_action(focus_item,argv[1],window);break;case SDLK_DOWN:scroll_line++;break;case SDLK_UP:if(scroll_line>0)scroll_line--;break;case SDLK_PAGEDOWN:scroll_line+=6;break;case SDLK_PAGEUP:scroll_line-=6;if(scroll_line<0)scroll_line=0;break;}}
        }
        draw();if(check){if(!status_text||!notes)return 1;puts("PASS: launcher background, UTF-8 text, notes and controls rendered");running=false;}SDL_Delay(33);
    }
    SDL_DestroyTexture(font);SDL_DestroyTexture(background);SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();return 0;
}
