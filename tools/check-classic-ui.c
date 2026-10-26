/* Link in place of main.c and modern_ui.c; requires an isolated marked copy.
 * Usage: check-classic-ui GAME CAPTURE_DIRECTORY [SAVE_SLOT] */
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
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);


static const char* capture_dir;
static void capture(const char* name)
{
    modern_ui_tick();iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface);
    char path[4096];snprintf(path,sizeof(path),"%s/%s.png",capture_dir,name);
    assert(SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
    printf("Capture %s, world %dx%d, modern=%d\n",name,hrp_iso_window_width_get(),hrp_iso_window_height_get(),enabled);
}
static void toggle(void)
{
    bool old=enabled;
    SDL_Event event;SDL_zero(event);event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_F9;
    assert(handle_event(&event));modern_ui_tick();assert(enabled!=old);SDL_Delay(180);
    event.key.repeat=true;assert(handle_event(&event));modern_ui_tick();assert(enabled!=old);
    event.type=SDL_EVENT_KEY_UP;event.key.repeat=false;assert(handle_event(&event));modern_ui_tick();assert(enabled!=old);
}
static tig_window_handle_t window_at(int x,int y,int bx,int by,int width,int height)
{
    tig_window_handle_t handle;TigWindowData data;
    assert(tig_window_get_at_position(x,y,&handle)==TIG_OK);
    assert(tig_window_data(handle,&data)==TIG_OK);
    assert(data.rect.x==bx&&data.rect.y==by&&data.rect.width==width&&data.rect.height==height);
    return handle;
}
static tig_window_handle_t sheet_window(void)
{
    int dx=(hrp_iso_window_width_get()-800)/2,dy=(hrp_iso_window_height_get()-600)/2;
    return window_at(dx+210,dy+240,dx,dy+41,800,400);
}
static tig_button_handle_t tab_button(tig_window_handle_t handle,int number)
{
    tig_button_handle_t* list;int count=tig_window_button_list(handle,&list);
    tig_art_id_t art;assert(tig_art_interface_id_create(number,0,0,0,&art)==TIG_OK);
    for(int i=0;i<count;i++){TigButtonData data;assert(tig_button_data(list[i],&data)==TIG_OK);if(data.art_id==art)return list[i];}
    assert(false);return TIG_BUTTON_HANDLE_INVALID;
}
static void click_native(tig_button_handle_t button)
{
    TigButtonData data;TigWindowData win;assert(tig_button_data(button,&data)==TIG_OK);assert(tig_window_data(data.window_handle,&win)==TIG_OK);
    int x=win.rect.x+data.x+data.width/2,y=win.rect.y+data.y+data.height/2;
    assert(tig_button_get_at_position(x,y)==button);
    TigMessage msg;memset(&msg,0,sizeof(msg));msg.type=TIG_MESSAGE_MOUSE;msg.data.mouse.x=x;msg.data.mouse.y=y;
    int events[]={TIG_MESSAGE_MOUSE_MOVE,TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN,TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP};
    for(int i=0;i<3;i++){
        msg.data.mouse.event=events[i];tig_message_enqueue(&msg);
        TigMessage pending;while(tig_message_dequeue(&pending)==TIG_OK)intgame_process_event(&pending);
        if(i==1)SDL_Delay(150);
    }
}
static uint64_t sheet_text_hash(tig_window_handle_t handle)
{
    TigVideoBuffer* buffer;TigVideoBufferData data;assert(tig_window_vbid_get(handle,&buffer)==TIG_OK);
    assert(tig_video_buffer_lock(buffer)==TIG_OK&&tig_video_buffer_data(buffer,&data)==TIG_OK);
    uint64_t hash=1469598103934665603ULL;
    /* Basic information and attributes, excluding the live character lens. */
    for(int y=15;y<260;y++)for(int x=140;x<480;x++){
        uint32_t pixel=*(uint32_t*)((char*)data.pixels+y*data.pitch+x*4);hash=(hash^pixel)*1099511628211ULL;
    }
    tig_video_buffer_unlock(buffer);return hash;
}
static uint64_t window_hash(tig_window_handle_t handle)
{
    TigVideoBuffer* buffer;TigVideoBufferData data;assert(tig_window_vbid_get(handle,&buffer)==TIG_OK);
    assert(tig_video_buffer_lock(buffer)==TIG_OK&&tig_video_buffer_data(buffer,&data)==TIG_OK);
    uint64_t hash=1469598103934665603ULL;
    for(int y=0;y<data.height;y++)for(int x=0;x<data.width;x++)hash=(hash^*(uint32_t*)((char*)data.pixels+y*data.pitch+x*4))*1099511628211ULL;
    tig_video_buffer_unlock(buffer);return hash;
}
static void resize(int width,int height)
{
    assert(SDL_SetWindowSize(window,width,height));modern_ui_tick();
    assert(hrp_iso_window_width_get()==width&&hrp_iso_window_height_get()==height);
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
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

    capture_dir=argv[2];
    assert(gamelib_load(argc==4?argv[3]:"Slot0002"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();

    capture("modern-hud");
    world_zoom=.75f;world_zoom_sync();assert(hrp_iso_window_width_get()>1360);
    toggle();assert(!enabled);capture("classic-hud");
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));modern_ui_tick();
    tig_window_handle_t sheet=sheet_window();
    uint64_t before=sheet_text_hash(sheet);
    tig_button_handle_t tabs[4];const int art[]={34,35,36,568};
    for(int i=0;i<4;i++)tabs[i]=tab_button(sheet,art[i]);
    const int sizes[][2]={{1601,1001},{800,600},{1920,1080},{1360,900}};
    for(int size=0;size<4;size++){
        resize(sizes[size][0],sizes[size][1]);assert(sheet_window()==sheet);charedit_refresh();
        assert(sheet_text_hash(sheet)==before);
        char shot[80];snprintf(shot,sizeof(shot),"classic-sheet-%dx%d",sizes[size][0],sizes[size][1]);capture(shot);
        for(int tab=0;tab<4;tab++){
            click_native(tabs[tab]);
            int dx=(sizes[size][0]-800)/2,dy=(sizes[size][1]-600)/2;
            tig_window_handle_t pane;TigWindowData data;
            assert(tig_window_get_at_position(dx+520,dy+120,&pane)==TIG_OK&&tig_window_data(pane,&data)==TIG_OK);
            assert(data.rect.x==dx+503&&data.rect.y==dy+104);
            if(size==0){snprintf(shot,sizeof(shot),"classic-sheet-tab-%d",tab);capture(shot);}
        }
        click_native(tabs[0]);
    }
    click_native(tabs[3]);resize(1600,1000);
    tig_window_handle_t pane;assert(tig_window_get_at_position(400+520,200+120,&pane)==TIG_OK);
    uint64_t scroll_before=window_hash(pane);
    TigMessage scroll={0};scroll.type=TIG_MESSAGE_MOUSE;scroll.data.mouse.x=400+503+217;scroll.data.mouse.y=200+104+290;
    scroll.data.mouse.event=TIG_MESSAGE_MOUSE_LEFT_BUTTON_DOWN;tig_message_enqueue(&scroll);
    TigMessage queued;while(tig_message_dequeue(&queued)==TIG_OK)intgame_process_event(&queued);
    SDL_Delay(150);scroll.data.mouse.event=TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;tig_message_enqueue(&scroll);
    while(tig_message_dequeue(&queued)==TIG_OK)intgame_process_event(&queued);
    assert(window_hash(pane)!=scroll_before);capture("classic-schemes-scrolled-after-resize");
    click_native(tabs[0]);resize(1360,900);
    toggle();assert(enabled&&charedit_is_created());capture("modern-sheet");
    toggle();assert(!enabled&&charedit_is_created());assert(sheet_window()==sheet);charedit_close();
    assert(inven_ui_open(pc,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));
    resize(1600,1000);capture("classic-inventory");
    toggle();assert(enabled&&inven_ui_is_created());capture("modern-inventory");
    toggle();assert(!enabled&&inven_ui_is_created());inven_ui_destroy();
    logbook_ui_open(pc);capture("classic-journal");logbook_ui_close();
    wmap_ui_open();capture("classic-map");wmap_ui_close();
    schematic_ui_toggle(pc,OBJ_HANDLE_NULL);capture("classic-schematics");schematic_ui_close();
    mainmenu_ui_start(MM_TYPE_IN_PLAY);capture("classic-pause");
    resize(1360,900);capture("classic-pause-resized");tig_window_handle_t covered;assert(tig_window_get_at_position(20,60,&covered)!=TIG_OK);sub_5412D0();modern_ui_tick();
    int dx=(1360-800)/2;window_at(dx+400,10,dx,0,800,41);window_at(dx+400,899,dx,900-159,800,159);
    capture("classic-hud-restored");toggle();capture("modern-hud-restored");
    puts("PASS: classic alignment, resize with open sheet, stable native text, all sheet tabs and hitboxes, F9 holds/releases, inventory, journal, map, schematics, pause and modern restoration");return 0;
}
