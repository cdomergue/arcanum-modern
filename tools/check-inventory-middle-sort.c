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
#include "game/proto.h"
#include "game/descriptions.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);

static void action(int id)
{
    pending_action=id;modern_ui_menu_tick();
}
static void render(const char* capture)
{
    button_count=0;SDL_SetRenderDrawColor(renderer,17,13,9,255);SDL_RenderClear(renderer);
    draw_menu();
    if(capture){SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,capture));SDL_DestroySurface(surface);}
}
static void click(int id)
{
    render(NULL);Button* target=NULL;
    for(int i=0;i<button_count;i++)if(buttons[i].action==id)target=&buttons[i];
    assert(target&&target->enabled);
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_Event event;SDL_zero(event);event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
    event.button.x=(target->rect.x+target->rect.w/2)*ww/canvas_w;
    event.button.y=(target->rect.y+target->rect.h/2)*wh/canvas_h;
    int old=settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY);
    assert(handle_event(&event));modern_ui_menu_tick();SDL_Delay(150);modern_ui_menu_tick();
    assert(settings_get_value(&settings,MODERN_FOLLOWER_AUTO_PICKUP_KEY)==old);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
    assert(handle_event(&event));modern_ui_menu_tick();
}
int main(int argc,char** argv)
{
    if(argc!=3||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
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
    canvas_w=1360;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,1360,900);
    assert(target&&SDL_SetRenderTarget(renderer,target));SDL_SetRenderLogicalPresentation(renderer,1360,900,SDL_LOGICAL_PRESENTATION_DISABLED);

    mainmenu_ui_window_type_set(MM_WINDOW_MAINMENU);mainmenu_ui_open();modern_ui_menu_tick();
    mainmenu_ui_close(false);mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(418);
    assert(options_tab==3);action(424);
    assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));
    click(480+KB_SORT);assert(key_edit==KB_SORT);
    SDL_Event key;SDL_zero(key);key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_B;
    assert(handle_event(&key));assert(key_edit==-1&&key_binding[KB_SORT]==SDL_SCANCODE_B);
    SDL_Delay(180);key.key.repeat=true;assert(handle_event(&key));key.key.repeat=false;
    /* Release after leaving options must remain owned by the binding editor. */
    mainmenu_ui_close(false);modern_ui_menu_tick();
    pending_action=-1;key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key)&&pending_action==-1);
    mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(418);
    key_binding[KB_SORT]=0;key_load();assert(key_binding[KB_SORT]==SDL_SCANCODE_B);
    click(480+KB_SORT);key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_I;
    assert(handle_event(&key)&&key_edit==KB_SORT&&*key_status);key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key));
    key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_ESCAPE;
    assert(handle_event(&key)&&key_edit==-1&&key_binding[KB_SORT]==SDL_SCANCODE_B);
    key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key));
    click(480+KB_SORT);key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_BACKSPACE;
    assert(handle_event(&key)&&key_binding[KB_SORT]==0);key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key));key_load();assert(key_binding[KB_SORT]==0);
    click(480+KB_SORT);
    SDL_Event bind_mouse;SDL_zero(bind_mouse);bind_mouse.type=SDL_EVENT_MOUSE_BUTTON_DOWN;bind_mouse.button.button=SDL_BUTTON_X1;bind_mouse.button.down=true;
    assert(handle_event(&bind_mouse)&&key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_X1));
    SDL_Delay(180);mainmenu_ui_close(false);modern_ui_menu_tick();pending_action=-1;
    bind_mouse.type=SDL_EVENT_MOUSE_BUTTON_UP;bind_mouse.button.down=false;assert(handle_event(&bind_mouse)&&pending_action==-1);
    mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(418);
    click(480+KB_SORT);bind_mouse.type=SDL_EVENT_MOUSE_BUTTON_DOWN;bind_mouse.button.down=true;bind_mouse.button.button=SDL_BUTTON_MIDDLE;
    assert(handle_event(&bind_mouse)&&key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));SDL_Delay(180);
    bind_mouse.type=SDL_EVENT_MOUSE_BUTTON_UP;bind_mouse.button.down=false;assert(handle_event(&bind_mouse));
    key_load();assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));
    render(argv[2]);for(int i=0;i<button_count;i++)assert(buttons[i].action!=425);
    mainmenu_ui_close(false);modern_ui_menu_tick();sub_5412D0();
    in_game=true;intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj(),dagger;
    assert(pc);object_flags_unset(pc,OF_OFF);
    assert(object_create(sub_4685A0(BP_DAGGER),obj_field_int64_get(pc,OBJ_F_LOCATION),&dagger));
    assert(item_transfer_ex(dagger,pc,80));assert(item_inventory_location_get(dagger)==80);
    assert(inven_ui_open(pc,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));panels_tick();
    button_count=panel_region_count=0;draw_inventory();assert(native_panel()==2);
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_Event middle;SDL_zero(middle);middle.type=SDL_EVENT_MOUSE_BUTTON_DOWN;middle.button.button=SDL_BUTTON_MIDDLE;middle.button.down=true;
    middle.button.x=(inventory_rect.x+500)*ww/canvas_w;middle.button.y=(inventory_rect.y+300)*wh/canvas_h;
    pending_action=-1;assert(handle_event(&middle));SDL_Delay(200);panels_tick();
    assert(pending_action==-1&&item_inventory_location_get(dagger)==80);
    middle.type=SDL_EVENT_MOUSE_BUTTON_UP;middle.button.down=false;assert(handle_event(&middle));
    assert(pending_action==731);int order=pending_action;pending_action=-1;assert(panels_action(order));
    assert(item_inventory_location_get(dagger)<80);
    int first=item_inventory_location_get(dagger);
    assert(handle_event(&middle)&&pending_action==-1&&item_inventory_location_get(dagger)==first);
    assert(key_assign(KB_SORT,SDL_SCANCODE_B));middle.type=SDL_EVENT_MOUSE_BUTTON_DOWN;middle.button.down=true;assert(handle_event(&middle));
    middle.type=SDL_EVENT_MOUSE_BUTTON_UP;middle.button.down=false;assert(handle_event(&middle)&&pending_action==-1);
    assert(item_transfer_ex(dagger,pc,80));assert(item_inventory_location_get(dagger)==80);
    key.type=SDL_EVENT_KEY_DOWN;key.key.scancode=SDL_SCANCODE_B;key.key.repeat=false;
    assert(handle_event(&key)&&pending_action==731);pending_action=-1;assert(panels_action(731));
    assert(item_inventory_location_get(dagger)<80);first=item_inventory_location_get(dagger);
    SDL_Delay(180);key.key.repeat=true;assert(handle_event(&key)&&pending_action==-1);
    key.type=SDL_EVENT_KEY_UP;key.key.repeat=false;assert(handle_event(&key)&&pending_action==-1&&item_inventory_location_get(dagger)==first);
    inventory_search_focus(true);key.type=SDL_EVENT_KEY_DOWN;assert(handle_event(&key)&&pending_action==-1);
    key.type=SDL_EVENT_KEY_UP;assert(handle_event(&key));inventory_search_focus(false);
    inven_ui_destroy();
    mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();modern_ui_menu_tick();action(418);
    key_load();assert(key_binding[KB_SORT]==SDL_SCANCODE_B);action(422);assert(key_binding[KB_SORT]==SDL_SCANCODE_B);
    action(424);assert(key_binding[KB_SORT]==(KEY_MOUSE|SDL_BUTTON_MIDDLE));
    puts("PASS: live keyboard/mouse rebinding, conflicts, cancellation, clear, persistence, delayed release across menus, held keys, search protection and actual item sorting");
    return 0;
}
