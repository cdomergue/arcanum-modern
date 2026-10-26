/* Link in place of main.c and modern_ui.c; requires an isolated marked copy.
 * Usage: check-button-text GAME CAPTURE_DIRECTORY [SAVE_SLOT] */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "ui/sleep_ui.h"
#include "ui/fate_ui.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);



static const char* capture_dir;
static void capture_frame(const char* name)
{
    modern_ui_tick();iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();
    if(name){
        SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface);
        char path[4096];snprintf(path,sizeof(path),"%s/%s.png",capture_dir,name);
        assert(SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    }
}
static void click_action(int action)
{
    capture_frame(NULL);int index=-1;
    for(int i=0;i<button_count;i++)if(buttons[i].action==action)index=i;
    assert(index>=0);
    int w,h;SDL_GetWindowSize(window,&w,&h);SDL_FRect r=buttons[index].rect;
    SDL_Event event;SDL_zero(event);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
    event.button.x=(r.x+r.w/2)*w/canvas_w;event.button.y=(r.y+r.h/2)*h/canvas_h;
    assert(handle_event(&event));modern_ui_tick();SDL_Delay(180);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;assert(handle_event(&event));modern_ui_tick();
}
static void check_labels(void)
{
    const char* labels[]={"Compétences technologiques","Technological Disciplines","Spell Colleges",
        "Rumeurs et notes","Rumors & Notes","Épée élémentaire enchantée","Arcane","ABCXYZsansaucunespaceallonge"};
    const int widths[]={109,122,180,28};
    for(int f=BUTTON_FONT;f<=MENU_BUTTON_FONT;f++)for(int i=0;i<8;i++)for(int j=0;j<4;j++) {
        ButtonTextLayout l=button_text_layout(f,labels[i],widths[j],36);
        assert(l.count>=1&&l.count<=2&&l.height<=36);
        for(int k=0;k<l.count;k++)assert(l.ink[k].w<=widths[j]);
        if(!l.shortened) {
            char combined[2048];SDL_strlcpy(combined,l.lines[0],sizeof(combined));
            if(l.count==2){SDL_strlcat(combined," ",sizeof(combined));SDL_strlcat(combined,l.lines[1],sizeof(combined));}
            assert(!strcmp(combined,labels[i]));
        }
    }
    float x=mouse_x,y=mouse_y;mouse_x=25;mouse_y=25;button_overflow_hint[0]=0;
    SDL_Rect old_clip={0,0,100,100},after;SDL_SetRenderClipRect(renderer,&old_clip);
    button_label(BUTTON_FONT,labels[0],(SDL_FRect){20,20,35,20},true,GOLD);
    assert(!strcmp(button_overflow_hint,labels[0]));assert(SDL_RenderClipEnabled(renderer));
    SDL_GetRenderClipRect(renderer,&after);assert(!memcmp(&old_clip,&after,sizeof(after)));
    SDL_SetRenderClipRect(renderer,NULL);mouse_x=x;mouse_y=y;
}
static void check_settings(void)
{
    FILE* file=fopen("modern-ui/settings.ini","w");assert(file);fputs("TextSize=24\n",file);fclose(file);
    load_settings();assert(body_size==24&&button_text_scale==100);
    file=fopen("modern-ui/settings.ini","w");assert(file);fputs("ButtonTextScale=150\nTextSize=22\n",file);fclose(file);
    load_settings();assert(body_size==22&&button_text_scale==150);
    file=fopen("modern-ui/settings.ini","w");assert(file);fputs("ButtonTextScale=10000\nTextSize=-2\n",file);fclose(file);
    load_settings();assert(body_size==20&&button_text_scale==100);
    body_size=24;button_text_scale=125;save_settings();body_size=16;button_text_scale=75;
    load_settings();assert(body_size==24&&button_text_scale==125);
    body_size=20;button_text_scale=100;save_settings();
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1360;init.height=768;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);

    capture_dir=argv[2];
    assert(gamelib_load(argc==4?argv[3]:"Slot0002"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();



    check_settings();
    const int scales[]={75,100,125,150};
    for(int s=0;s<4;s++){
        button_text_scale=scales[s];update_geometry();check_labels();
        assert(fonts[0].size==14&&fonts[1].size==20&&fonts[2].size==22&&fonts[3].size==30);
        assert(fonts[BUTTON_FONT].size==14*scales[s]/100.0f);
        char name[80];
        assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));
        snprintf(name,sizeof(name),"sheet-%d",scales[s]);capture_frame(name);charedit_close();
        logbook_ui_open(pc);assert(journal);
        snprintf(name,sizeof(name),"journal-%d",scales[s]);capture_frame(name);
        if(scales[s]==150) {
            for(int i=0;i<button_count;i++)if(buttons[i].action==103) {
                SDL_FRect r=buttons[i].rect;int w,h;SDL_GetWindowSize(window,&w,&h);
                SDL_WarpMouseInWindow(window,(r.x+r.w/2)*w/canvas_w,(r.y+r.h/2)*h/canvas_h);break;
            }
            capture_frame("journal-overflow-hover");
            assert(!strcmp(button_overflow_hint,game_text(modern_logbook_tab_label(3))));
            SDL_WarpMouseInWindow(window,0,0);
        }
        logbook_ui_close();
    }
    mainmenu_ui_start(MM_TYPE_IN_PLAY);modern_ui_tick();click_action(502);
    assert(mainmenu_ui_window_type_get()==MM_WINDOW_OPTIONS);capture_frame("graphics-150");
    click_action(426);assert(button_text_scale==125);
    click_action(426);assert(button_text_scale==100);
    click_action(426);assert(button_text_scale==75);capture_frame("graphics-75");
    click_action(428);assert(button_text_scale==100);
    click_action(427);assert(button_text_scale==125);
    click_action(427);assert(button_text_scale==150);
    button_text_scale=100;load_settings();assert(button_text_scale==150&&body_size==20);
    capture_frame("graphics-persisted-150");
    click_action(428);assert(button_text_scale==100);capture_frame("graphics-default");
    /* Check atlas packing and logical bounds at common low/high DPI outputs. */
    const int sizes[][2]={{800,600},{1920,1080},{3840,2160}};
    for(int i=0;i<3;i++) {
        assert(SDL_SetWindowSize(window,sizes[i][0],sizes[i][1]));
        button_text_scale=150;update_geometry();check_labels();
        for(int f=0;f<FONT_COUNT;f++)assert(fonts[f].texture&&fonts[f].density==density);
        assert(fonts[4].size==63&&fonts[MENU_BUTTON_FONT].size==33);
    }
    button_text_scale=100;save_settings();
    puts("PASS: real font bounds, French/English wrapping, complete hover labels, 75–150%, click holds, persisted preferences and independent body fonts");
    return 0;
}
