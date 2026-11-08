/* Live X11 regression: use only a marked disposable game copy. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <dlfcn.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/descriptions.h"
#include "game/map.h"
#include "game/obj.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);
static void* display;
static int (*fake_key)(void*,unsigned int,int,unsigned long);
static int (*flush)(void*);
static unsigned int keycode;
static void key(bool down){assert(fake_key(display,keycode,down,0));flush(display);}
static void require_check(bool ok,const char* condition,int line)
{
    if(ok)return;
    if(display&&fake_key&&flush&&keycode){fake_key(display,keycode,0,0);flush(display);}
    fprintf(stderr,"FAIL line %d: %s\n",line,condition);exit(1);
}
#undef assert
#define assert(condition) require_check((condition),#condition,__LINE__)
static void frames(int milliseconds)
{
    Uint64 end=SDL_GetTicks()+milliseconds;int iterations=0;
    do {
        SDL_Event e;while(SDL_PollEvent(&e)){if(e.type==SDL_EVENT_KEY_DOWN||e.type==SDL_EVENT_KEY_UP)printf("KEY type=%u scan=%d down=%d\n",e.type,e.key.scancode,e.key.down);handle_event(&e);}
        modern_ui_tick();gamelib_ping();modern_follow_camera_tick();
        iso_redraw();tig_window_display();SDL_Delay(16);
    } while(++iterations<3 || SDL_GetTicks()<end);
}
int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"x11",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_FRAMEBUFFER_ACCELERATION,"0",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1280;init.height=720;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    if(!strcmp(argv[2],"--fresh")) {
        int start=map_by_type(MAP_TYPE_START_MAP);char* name;int64_t x,y;
        assert(map_get_name(start,&name)&&map_get_starting_location(start,&x,&y));
        assert(map_preprocess_mobile(name));
        char map_path[512];snprintf(map_path,sizeof(map_path),"maps\\%s",name);
        assert(map_open(map_path,"follow-input-fixture",false));
        PlayerCreateInfo info;player_create_info_init(&info);
        info.basic_prototype=16066;info.loc=location_make(x,y);
        assert(player_obj_create_player(&info));
        /* Keep the fixture free of introduction dialogs and hostile AI. */
        ObjectList nearby;object_list_vicinity(info.obj,OBJ_TM_NPC,&nearby);
        for(ObjectNode* n=nearby.head;n;n=n->next)object_flags_set(n->obj,OF_OFF);
        object_list_destroy(&nearby);
        int64_t follower;
        assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(x+2,y),&follower));
        assert(critter_follow(follower,info.obj,true));
    } else assert(gamelib_load(argv[2]));
    sub_5412D0();in_game=true;enabled=true;
    intgame_mode_set(INTGAME_MODE_MAIN);follow_camera=true;key_binding[KB_UP]=SDL_SCANCODE_W;
    int64_t pc=player_get_local_pc_obj(),allies[16];object_flags_unset(pc,OF_OFF);
    int count=modern_followers_get(allies,16);assert(count>0);
    int64_t companion=allies[0];assert(modern_party_select(companion));
    assert(follow_control_actor()==companion);
    location_origin_set(obj_field_int64_get(companion,OBJ_F_LOCATION));iso_interface_refresh();
    SDL_PropertiesID props=SDL_GetWindowProperties(window);
    display=SDL_GetPointerProperty(props,SDL_PROP_WINDOW_X11_DISPLAY_POINTER,NULL);
    unsigned long xid=SDL_GetNumberProperty(props,SDL_PROP_WINDOW_X11_WINDOW_NUMBER,0);assert(display&&xid);
    printf("TEST pid=%ld SDL_window=%u X11_window=%lu selected=%llu hero=%llu\n",(long)getpid(),SDL_GetWindowID(window),xid,(unsigned long long)companion,(unsigned long long)pc);
    void* x=dlopen("libX11.so.6",RTLD_NOW),*xt=dlopen("libXtst.so.6",RTLD_NOW);assert(x&&xt);
    int (*set_focus)(void*,unsigned long,int,unsigned long)=dlsym(x,"XSetInputFocus");
    unsigned char (*keycode_for)(void*,unsigned long)=dlsym(x,"XKeysymToKeycode");
    flush=dlsym(x,"XFlush");fake_key=dlsym(xt,"XTestFakeKeyEvent");assert(set_focus&&keycode_for&&flush&&fake_key);
    SDL_ShowWindow(window);SDL_RaiseWindow(window);set_focus(display,xid,1,0);flush(display);frames(200);
    printf("GATE focus=%d enabled=%d ingame=%d menu=%d journal=%d panel=%d slide=%d search=%d suspended=%d mode=%d\n",SDL_GetKeyboardFocus()==window,enabled,in_game,mainmenu_ui_is_active(),journal,native_panel(),slide_ui_is_active(),searching,menu_suspended,intgame_mode_get());
    assert(SDL_GetKeyboardFocus()==window && modern_camera_enabled());
    keycode=keycode_for(display,SDL_GetKeyFromScancode(SDL_SCANCODE_W,0,false));assert(keycode);
    /* Find an unobstructed movement direction for this fixture. */
    SDL_Delay(1100);
    int64_t tile=obj_field_int64_get(companion,OBJ_F_LOCATION);int chosen=-1;
    const int scans[]={SDL_SCANCODE_W,SDL_SCANCODE_D,SDL_SCANCODE_S,SDL_SCANCODE_A};
    const int dirs[]={0,2,4,6};
    for(int i=0;i<4;i++){int64_t dest;AnimPath path;
        bool found=location_in_range(tile,dirs[i],1,&dest)&&anim_preview_path(companion,dest,false,&path);
        printf("PATH direction=%d found=%d steps=%d active=%d speed=%d\n",dirs[i],found,path.max,critter_is_active(companion),stat_level_get(companion,STAT_SPEED));
        if(found&&path.max==1){
            bool straight=true;for(int j=0;j<path.max;j++)if(path.rotations[j]!=dirs[i])straight=false;
            if(straight){chosen=i;break;}
        }
    }
    assert(chosen>=0);
    key_binding[KB_UP]=SDL_SCANCODE_W;key_binding[KB_UP+1]=SDL_SCANCODE_A;
    key_binding[KB_UP+2]=SDL_SCANCODE_S;key_binding[KB_UP+3]=SDL_SCANCODE_D;
    keycode=keycode_for(display,SDL_GetKeyFromScancode(scans[chosen],0,false));assert(keycode);
    key(true);frames(250);
    printf("INPUT keycode=%u scancode=%d held=%d follow=%d compact=%d tactical=%d focus=%d actor=%llu selected=%llu\n",keycode,scans[chosen],SDL_GetKeyboardState(NULL)[scans[chosen]],modern_follow_camera_active(),intgame_is_compact_interface(),tactical_active(),SDL_GetKeyboardFocus()==window,(unsigned long long)follow_walk.pc,(unsigned long long)modern_party_selected());
    assert(follow_walk.pc==companion);
    assert(obj_field_int64_get(companion,OBJ_F_LOCATION)!=tile || obj_field_int32_get(companion,OBJ_F_OFFSET_X) || obj_field_int32_get(companion,OBJ_F_OFFSET_Y));
    /* Selection changes while the physical key state stays down. */
    assert(modern_party_select(0));frames(200);assert(follow_control_actor()==pc && follow_walk.pc!=companion);
    assert(modern_party_select(companion));frames(200);assert(follow_walk.pc==companion);
    key(false);frames(200);assert(!follow_walk.pc);
    key(true);frames(200);assert(follow_walk.pc==companion);
    mainmenu_ui_start(MM_TYPE_IN_PLAY);frames(200);assert(mainmenu_ui_is_active()&&!follow_walk.pc);
    key(false);frames(200);sub_5412D0();frames(200);assert(!mainmenu_ui_is_active()&&!follow_walk.pc);
    assert(modern_party_select(companion));
    key(true);frames(200);assert(follow_walk.pc==companion);key(false);frames(200);assert(!follow_walk.pc);
    SDL_Surface* capture=SDL_RenderReadPixels(renderer,NULL);assert(capture&&SDL_SavePNG(capture,"follow-input-check.png"));SDL_DestroySurface(capture);
    puts("PASS: live selected companion movement, held-key selection switches, delayed release and pause menu transitions");
    return 0;
}
