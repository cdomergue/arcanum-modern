/* Render and verify HP feedback in a disposable, marked game copy. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
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
static int count(void){int n=0;for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++)if(health_floats[i].delta)n++;return n;}
static void capture(const char* path)
{
    iso_redraw();tig_window_invalidate_rect(NULL);tig_window_display();
    SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface);
    assert(SDL_SavePNG(surface,path));SDL_DestroySurface(surface);
}
int main(int argc,char** argv)
{
    if(argc!=2 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=1280;init.height=720;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    int start=map_by_type(MAP_TYPE_START_MAP);char* name;int64_t x,y;
    assert(map_get_name(start,&name)&&map_get_starting_location(start,&x,&y));
    assert(map_preprocess_mobile(name));char map_path[512];snprintf(map_path,sizeof(map_path),"maps\\%s",name);
    assert(map_open(map_path,"floating-health-fixture",false));
    PlayerCreateInfo info;player_create_info_init(&info);info.basic_prototype=16066;info.loc=location_make(x,y);
    assert(player_obj_create_player(&info));
    ObjectList nearby;object_list_vicinity(info.obj,OBJ_TM_NPC,&nearby);
    for(ObjectNode* n=nearby.head;n;n=n->next)object_flags_set(n->obj,OF_OFF);object_list_destroy(&nearby);
    int64_t ally;assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(x+4,y),&ally));
    assert(critter_follow(ally,info.obj,true));sub_5412D0();in_game=true;enabled=true;
    intgame_mode_set(INTGAME_MODE_MAIN);object_flags_unset(info.obj,OF_OFF);
    location_origin_set(info.loc);iso_interface_refresh();modern_ui_tick();
    printf("TEST pid=%ld SDL_window=%u isolated=%s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    capture("floating-health-before.png");
    modern_health_reset();
    object_hp_damage_set(info.obj,7);object_hp_damage_set(ally,12);
    CombatContext heal={0};heal.attacker_obj=info.obj;heal.target_obj=ally;heal.dam[DAMAGE_TYPE_NORMAL]=5;
    combat_heal(&heal);assert(object_hp_damage_get(ally)==7);assert(count()==3);
    capture("floating-health-start.png");
    /* Sample deterministic animation phases independently of machine rendering speed. */
    for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++)if(health_floats[i].delta)health_floats[i].born=SDL_GetTicks()-650;
    capture("floating-health-fade.png");
    SDL_Delay(600);capture("floating-health-expired.png");assert(!count());
    world_zoom=.75f;world_zoom_sync();modern_health_change(ally,0,12);assert(count()==1);
    capture("floating-health-zoom.png");
    modern_health_reset();assert(!count());
    int remaining=object_hp_current(ally);assert(remaining>0);
    critter_kill(ally);assert(object_hp_damage_get(ally)==32000);
    assert(count()==1);
    for(int i=0;i<HEALTH_FLOAT_CAPACITY;i++)if(health_floats[i].delta)assert(health_floats[i].delta==-remaining);
    modern_health_reset();
    object_hp_damage_set(ally,8);assert(count()==1);object_reset();assert(!count());
    puts("PASS: real HP damage and combat healing, simultaneous actors, rendered rise/fade/expiry, zoom and map reset");
    return 0;
}
