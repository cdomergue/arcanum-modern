/* Link in place of main.c and modern_ui.c. Use only an isolated marked copy. */
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
#include <sys/stat.h>
static int questions,opened,answer;
static bool visual;
static bool test_question(const SDL_MessageBoxData* question,int* choice);
static bool test_open_url(const char* url);
#define SDL_ShowMessageBox test_question
#define SDL_OpenURL test_open_url
#include "../source/src/ui/modern_ui.c"
#undef SDL_ShowMessageBox
#undef SDL_OpenURL
static bool test_question(const SDL_MessageBoxData* q,int* choice)
{
    assert(q->numbuttons==2 && q->buttons[0].buttonID==0 && q->buttons[1].buttonID==1);
    assert(q->buttons[0].flags & SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT);
    assert(q->buttons[0].flags & SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT);
    assert(strstr(q->message,ARCANUM_RENAISSANCE_VERSION) && strstr(q->message,"v9.0.0"));
    assert(!strcmp(q->buttons[0].text,"Ignorer"));
    questions++;*choice=answer;
    printf("Invitation: %s / %s\n",q->buttons[0].text,q->buttons[1].text);
    if(visual && questions==1) {
        SDL_MessageBoxData shown=*q;shown.window=NULL;
        shown.title="Arcanum Renaissance — test de mise à jour";
        assert(SDL_ShowMessageBox(&shown,choice));
    }
    return true;
}
static bool test_open_url(const char* url)
{
    assert(!strcmp(url,"https://github.com/cdomergue/arcanum-modern/releases/tag/v9.0.0"));
    opened++;return true;
}
static void fixture(const char* output)
{
    update_close();update_started=false;
    assert(SDL_CreateDirectory("ArcanumModern/uap"));
    FILE* f=fopen(UPDATE_HELPER,"w");assert(f);
    fprintf(f,"#!/bin/sh\n[ \"$1\" = --mode ] && [ \"$2\" = check-update ] && [ \"$3\" = --current-version ] && [ \"$4\" = " ARCANUM_RENAISSANCE_VERSION " ] || exit 1\nsleep 0.1\nprintf '%%s\\n' '%s'\n",output);
    assert(!fclose(f)&&!chmod(UPDATE_HELPER,0700));
}
static void complete(void)
{
    Uint64 end=SDL_GetTicks()+3000;
    do {modern_ui_menu_tick();SDL_Delay(10);}while(update_process && SDL_GetTicks()<end);
    assert(!update_process);
}
void sub_549A70(void);

int main(int argc,char** argv)
{
    if((argc!=2 && argc!=3)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    visual=argc==3 && !strcmp(argv[2],"visual");
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,visual?"x11":"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1360;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    canvas_w=1360;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1360,900);
    assert(target&&SDL_SetRenderTarget(renderer,target));SDL_SetRenderLogicalPresentation(renderer,1360,900,SDL_LOGICAL_PRESENTATION_DISABLED);
    update_started=true;
    mainmenu_ui_window_type_set(MM_WINDOW_MAINMENU);mainmenu_ui_open();modern_ui_menu_tick();
    /* The helper fixture is private to this marked sandbox. */
    fixture("v9.0.0");answer=0;
    Uint64 before=SDL_GetTicks();modern_ui_menu_tick();
    assert(SDL_GetTicks()-before<100 && update_process && !questions);
    complete();assert(questions==1 && opened==0);
    for(int i=0;i<10;i++)modern_ui_menu_tick();assert(questions==1);
    fixture("");complete();assert(questions==1); /* offline / already current */
    fixture("v9.0.0/../../evil");complete();assert(questions==1);
    fixture("v9.0.0");answer=1;complete();assert(questions==2 && opened==1);
    fixture("v9.0.0");click_captured=true;pressed_action=400;
    complete();assert(questions==2 && *update_tag);
    SDL_Delay(150);modern_ui_menu_tick();assert(questions==2);
    click_captured=false;pressed_action=-1;modern_ui_menu_tick();assert(questions==3);
    fixture("v9.0.0");modern_ui_menu_tick();assert(update_process);
    update_deadline=0;modern_ui_menu_tick();assert(!update_process && !*update_tag && questions==3);
    fixture("v9.0.0");enabled=false;complete();assert(questions==4);enabled=true;
    fixture("v9.0.0");modern_ui_menu_tick();assert(update_process);
    in_game=true;modern_ui_menu_tick();assert(!update_process && questions==4);in_game=false;
    fixture("v9.0.0");modern_ui_menu_tick();assert(update_process);update_close();
    assert(!update_process);
    mainmenu_ui_close(false);mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();
    update_started=false;modern_ui_menu_tick();assert(!update_started);
    unlink(UPDATE_HELPER);
    puts("PASS: asynchronous startup, ignore/open, no repeat, empty/invalid results, held click, timeout, classic UI and cleanup");
    return 0;
}
