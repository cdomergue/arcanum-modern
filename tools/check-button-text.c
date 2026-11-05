/* Link in place of main.c and modern_ui.c; requires an isolated marked copy.
 * Usage: check-button-text GAME CAPTURE_DIRECTORY [SAVE_SLOT] */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#ifdef _WIN32
#include <io.h>
#include <direct.h>
#include <process.h>
#define chdir _chdir
#define access _access
#define getpid _getpid
#define R_OK 4
#else
#include <unistd.h>
#endif
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "ui/sleep_ui.h"
#include "ui/fate_ui.h"
#include "../source/src/ui/modern_ui.c"
/* Avoid a modal CRT assertion dialog in unattended native capture jobs. */
#undef assert
#define assert(condition) do { if(!(condition)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition);exit(3);} } while(0)
void sub_549A70(void);
void sub_5412D0(void);



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
    load_settings();assert(text_scale==125);
    file=fopen("modern-ui/settings.ini","w");assert(file);fputs("ButtonTextScale=150\nTextSize=22\n",file);fclose(file);
    load_settings();assert(text_scale==150);
    file=fopen("modern-ui/settings.ini","w");assert(file);fputs("TextScale=75\nButtonTextScale=150\nTextSize=26\n",file);fclose(file);
    load_settings();assert(text_scale==75);
    file=fopen("modern-ui/settings.ini","w");assert(file);fputs("ButtonTextScale=10000\nTextSize=-2\nTextScale=88\n",file);fclose(file);
    load_settings();assert(text_scale==100);
    text_scale=125;save_settings();text_scale=75;
    load_settings();assert(text_scale==125);
    text_scale=100;save_settings();
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IONBF,0);
#ifdef _WIN32
    _set_error_mode(_OUT_TO_STDERR);
#endif
    puts("Starting isolated typography capture");
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1360;init.height=768;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    puts("Initialize TIG");assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    puts("Initialize game");GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    puts("Initialize native interface");gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);

    capture_dir=argv[2];
    assert(gamelib_load(argc==4?argv[3]:"Slot0002"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();



    check_settings();
    const int scales[]={75,100,125,150};
    for(int s=0;s<4;s++){
        text_scale=scales[s];update_geometry();check_labels();
        assert(fonts[0].size==text_size()&&fonts[1].size==text_size());
        assert(fonts[2].size==24*scales[s]/100.0f&&fonts[3].size==30*scales[s]/100.0f);
        assert(fonts[BUTTON_FONT].size==text_size()&&fonts[MENU_BUTTON_FONT].size==text_size());
        char name[80];
        int64_t followers[128];int follower_count=modern_followers_get(followers,128);
        int64_t inspected=follower_count?followers[0]:pc;
        char object_name[MAX_STRING];object_examine(inspected,pc,object_name);object_hover_obj_set(inspected);
        assert(modern_examine_begin(pc,inspected));intgame_examine_object(pc,inspected,object_name);modern_examine_end();
        assert(modern_hover_details());
        snprintf(name,sizeof(name),"hud-%d",scales[s]);capture_frame(name);
        object_hover_obj_set(OBJ_HANDLE_NULL);
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
        if(scales[s]==125){
            click_action(203);assert(text_scale==150);capture_frame(NULL);assert(fonts[0].size==30);
            click_action(202);assert(text_scale==125);
            text_scale=75;load_settings();assert(text_scale==125);
        }
        logbook_ui_close();
        assert(inven_ui_create(pc,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));
        snprintf(name,sizeof(name),"inventory-%d",scales[s]);capture_frame(name);inven_ui_destroy();
        if(follower_count){
            dialog_ui_start_dialog(pc,followers[0],1079,0,1);
            assert(modern_dialog_state() && modern_dialog_state()->num_options>0);
            if(modern_dialog_state()){
                snprintf(name,sizeof(name),"dialogue-%d",scales[s]);capture_frame(name);
                assert(dialogue_view.y+dialogue_view.h+16<bottom_bar_rect(0,0,0,0).y);
                dialog_ui_end_dialog(pc,0);
            }
        }
    }
    mainmenu_ui_start(MM_TYPE_IN_PLAY);modern_ui_tick();click_action(502);
    assert(mainmenu_ui_window_type_get()==MM_WINDOW_OPTIONS);capture_frame("graphics-150");
    click_action(426);assert(text_scale==125);
    click_action(426);assert(text_scale==100);
    click_action(426);assert(text_scale==75);capture_frame("graphics-75");
    click_action(428);assert(text_scale==100);
    click_action(427);assert(text_scale==125);
    click_action(427);assert(text_scale==150);
    text_scale=100;load_settings();assert(text_scale==150);
    capture_frame("graphics-persisted-150");
    click_action(428);assert(text_scale==100);capture_frame("graphics-default");
    /* Check atlas packing and logical bounds at common low/high DPI outputs. */
    const int sizes[][2]={{800,600},{1920,1080},{3840,2160}};
    for(int i=0;i<3;i++) {
        assert(SDL_SetWindowSize(window,sizes[i][0],sizes[i][1]));
        text_scale=150;update_geometry();check_labels();
        for(int f=0;f<FONT_COUNT;f++)assert(fonts[f].texture&&fonts[f].density==density);
        SDL_FRect content=bottom_bar_message_rect();
        assert(content.h>=3*text_line_height());
        assert(bottom_bar_rect(0,0,BOTTOM_BAR_SOURCE_WIDTH,BOTTOM_BAR_SOURCE_HEIGHT).x>=0);
        char size_name[80];snprintf(size_name,sizeof(size_name),"graphics-%dx%d-150",sizes[i][0],sizes[i][1]);capture_frame(size_name);
        assert(fonts[4].size==63&&fonts[MENU_BUTTON_FONT].size==30);
    }
    /* Inspect the narrowest supported canvas as well as the options screen. */
    assert(SDL_SetWindowSize(window,800,600));text_scale=150;update_geometry();
    mainmenu_ui_close(false);intgame_mode_set(INTGAME_MODE_MAIN);
    logbook_ui_open(pc);capture_frame("journal-800x600-150");logbook_ui_close();
    assert(inven_ui_create(pc,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));
    capture_frame("inventory-800x600-150");inven_ui_destroy();
    text_scale=100;save_settings();
    puts("PASS: real font bounds, French/English wrapping, complete hover labels, 75–150%, click holds, persisted preferences and shared reading fonts");
    return 0;
}
