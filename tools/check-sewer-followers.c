/* Link in place of main.c and follower_ui.c; only use an isolated marked copy.
 * Usage: check-sewer-followers GAME [SAVE_SLOT] */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/anim.h"
#include "game/tile.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/iso.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/follower_ui.h"
#include "game/teleport.h"
#include "game/map.h"
#include "../source/src/ui/follower_ui.c"
void sub_549A70(void);
int main(int argc,char** argv)
{
    if((argc!=2&&argc!=3)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
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
    SDL_Renderer* renderer;assert(tig_video_renderer_get(&renderer)==TIG_OK);
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(SDL_GetRenderWindow(renderer)),argv[1]);


    assert(gamelib_load(argc==3?argv[2]:"SlotA035"));sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));iso_interface_refresh();




    ObjectID sogg_id={0};
    if(argc==3 && !strcmp(argv[2],"SlotA034")) {
        int64_t followers[8];int count=modern_followers_get(followers,8);assert(count==3);
        int64_t sogg=followers[1];sogg_id=obj_get_id(sogg);
        char name[256];object_examine(sogg,pc,name);printf("Separated follower: %s\n",name);
        assert(modern_party_select(sogg));
        TeleportData first={0};first.obj=sogg;first.map=1;first.loc=281749854679668LL;
        assert(teleport_do(&first));teleport_ping(0);
        assert(map_current_map()==16);
        assert(!modern_party_set_grouped(sogg,true));
        follower_ui_refresh();assert(follower_ui_followers_count==2);
        assert(modern_followers_get(followers,8)==2);
        assert(!modern_party_selected());
        modern_party_select(OBJ_HANDLE_NULL);
    }
    else {
        /* Saving again while Sogg is on the destination map must not erase him. */
        assert(gamelib_save("SwrTest","Sewer transition regression"));
        assert(gamelib_load("SwrTest"));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
        pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    }
    printf("Before teleport: map=%d, followers=%d\n",map_current_map(),follower_ui_followers_count);
    for(int i=0;i<follower_ui_followers_count;i++) {
        int64_t obj=follower_ui_followers[i].obj;
        printf("slot %d: obj=%lld valid=%d oid=%d",i,(long long)obj,obj_handle_is_valid(obj),follower_ui_followers[i].field_8.objid.type);
        if(obj&&obj_handle_is_valid(obj))printf(" type=%d flags=%x fatigue=%d",obj_field_int32_get(obj,OBJ_F_TYPE),obj_field_int32_get(obj,OBJ_F_FLAGS),critter_fatigue_max(obj));
        puts("");
    }
    TeleportData transfer={0};transfer.obj=pc;transfer.map=1;transfer.loc=281749854679668LL;
    assert(teleport_do(&transfer));teleport_ping(0);
    printf("After teleport: map=%d, followers=%d\n",map_current_map(),follower_ui_followers_count);
    for(int i=0;i<follower_ui_followers_count;i++) {
        int64_t obj=follower_ui_followers[i].obj;
        printf("slot %d: obj=%lld valid=%d oid=%d\n",i,(long long)obj,obj_handle_is_valid(obj),follower_ui_followers[i].field_8.objid.type);
    }
    follower_ui_update();follower_ui_refresh();
    int64_t followers[8];int found=modern_followers_get(followers,8);
    printf("Resolved followers: %d, raw slots: %d\n",found,obj_arrayfield_length_get(player_get_local_pc_obj(),OBJ_F_CRITTER_FOLLOWER_IDX));
    for(int i=0;i<found;i++){char name[256];object_examine(followers[i],player_get_local_pc_obj(),name);printf("  %s\n",name);}
    assert(found==3);
    assert(follower_ui_followers_count==3);
    for(int i=0;i<3;i++) {
        assert(obj_handle_is_valid(followers[i]));
        assert(obj_field_int32_get(followers[i],OBJ_F_TYPE)==OBJ_TYPE_NPC);
        assert(!(obj_field_int32_get(followers[i],OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT)));
        /* The same HP change callback as a healing spell in the crash stack. */
        object_hp_damage_set(followers[i],object_hp_damage_get(followers[i]));
    }
    if(sogg_id.type!=OID_TYPE_NULL) {
        int64_t sogg=obj_pool_perm_lookup(sogg_id);assert(sogg&&modern_party_detached(sogg));
        assert(modern_party_set_grouped(sogg,true)&&!modern_party_detached(sogg));
    }
    modern_followers_sync(false);follower_ui_update();modern_followers_sync(true);follower_ui_update();
    puts("PASS: sewer exit, all three companions, healing callbacks, native/modern bars and regrouping");
    return 0;
}
