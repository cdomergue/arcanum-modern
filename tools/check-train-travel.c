// Native integration check. Link in place of teleport.o; run with an isolated
// game directory (.map-export-sandbox marker) and a save containing followers.
#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#define chdir _chdir
#define access _access
#define R_OK 4
#else
#include <unistd.h>
#endif
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/location.h"
#include "game/sector.h"
#include "game/townmap.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include "ui/follower_ui.h"
#include "game/combat.h"
#include "game/critter.h"
#include "game/dialog.h"
#include "game/stat.h"
#include "game/quest.h"
#include "ui/skill_ui.h"
#include "ui/spell_ui.h"
#include "ui/inven_ui.h"
#include "game/map.h"
#include "game/jumppoint.h"
#include "game/teleport.h"
#include "game/obj_pool.h"
#include "game/spell.h"
#include "game/script_name.h"
#include "game/script.h"
#include "game/gmovie.h"
#include "ui/modern_movie.h"

static int movies;
static bool skip_movie;
static Uint32 SDLCALL skip(void* data, SDL_TimerID id, Uint32 interval)
{
    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.key = SDLK_ESCAPE;
    event.key.scancode = SDL_SCANCODE_ESCAPE;
    event.key.down = true;
    SDL_PushEvent(&event);
    return 0;
}
static void watch_movie(const char* path, GameMovieFlags flags, int track)
{
    assert(strcmp(path, "ArcanumModern\\movies\\train_leaving.bik") == 0);
    movies++;
    if (skip_movie) assert(SDL_AddTimer(200, skip, NULL));
    Uint64 start = SDL_GetTicks();
    gmovie_play_path(path, flags, track);
    Uint64 elapsed = SDL_GetTicks() - start;
    assert(skip_movie ? elapsed >= 150 && elapsed < 2000 : elapsed > 1000);
}
#define gmovie_play_path watch_movie
#include "../source/src/game/teleport.c"
#undef gmovie_play_path

int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc<3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};
    init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;
    init.texture_width=init.texture_height=2048;
    init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;
    init.sound_file_path_resolver=gsound_resolve_path;
    if(tig_init(&init)!=TIG_OK)return 3;
    intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};
    if(!intgame_create_iso_window(&game.iso_window_handle) || !gamelib_init(&game))return 4;
    if(!gameuilib_init(&game) || !gamelib_mod_load("Arcanum") || !gameuilib_mod_load())return 5;
    gamelib_splash_clear();
    if(!modern_ui_init())return 6;
    modern_movie_init();
    assert(gamelib_load(argv[2]));
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(!sub_45B300());

    ObjectID followers[32];
    int inventories[32], follower_count = 0;
    ObjectList party;
    object_list_followers(player_get_local_pc_obj(), &party);
    for (ObjectNode* node = party.head; node; node = node->next) {
        assert(follower_count < 32);
        followers[follower_count] = obj_get_id(node->obj);
        inventories[follower_count++] = obj_field_int32_get(node->obj, OBJ_F_CRITTER_INVENTORY_NUM);
    }
    object_list_destroy(&party);
    assert(follower_count > 0);
    int scripts[] = {1748, 1752, 1753, 1754, 2237, 2238};
    int xs[] = {32322, 62067, 32289, 62089, 70135, 70156};
    int ys[] = {62362, 65566, 62360, 65565, 92060, 92060};
    for (int i=0;i<6;i++) {
        int64_t pc=player_get_local_pc_obj();
        Script script={0}; script.num=scripts[i];
        ScriptInvocation invocation={0};
        invocation.script=&script; invocation.line=10;
        invocation.triggerer_obj=pc; invocation.attachee_obj=pc;
        invocation.attachment_point=SAP_USE;
        skip_movie=i!=0;
        script_execute(&invocation);
        assert(teleport_pending);
        assert(current_teleport_data.sound_id==8810);
        assert(current_teleport_data.loc==location_make(xs[i],ys[i]));
        teleport_ping(0);
        assert(movies==i+1);
        assert(map_current_map()==1);
        pc=player_get_local_pc_obj();
        assert(obj_field_int64_get(pc,OBJ_F_LOCATION)==location_make(xs[i],ys[i]));
        assert(!teleport_pending && !tig_movie_is_playing());
        for (int f = 0; f < follower_count; f++) {
            int64_t follower = obj_pool_perm_lookup(followers[f]);
            assert(obj_handle_is_valid(follower));
            assert((obj_field_int32_get(follower, OBJ_F_FLAGS) & (OF_OFF | OF_DESTROYED | OF_EXTINCT)) == 0);
            assert(critter_leader_get(follower) == pc);
            assert(obj_field_int32_get(follower, OBJ_F_CRITTER_INVENTORY_NUM) == inventories[f]);
        }
        printf("PASS train script %d: movie %s, correct arrival\n",scripts[i],skip_movie?"skipped":"complete");
    }
    TeleportData ordinary={0};
    ordinary.obj=player_get_local_pc_obj();
    ordinary.map=map_current_map();
    ordinary.loc=obj_field_int64_get(ordinary.obj,OBJ_F_LOCATION);
    assert(teleport_do(&ordinary)); teleport_ping(0); assert(movies==6);
    ordinary.flags=TELEPORT_SOUND|TELEPORT_TIME;
    ordinary.sound_id=8811; ordinary.time=1;
    assert(teleport_do(&ordinary)); teleport_ping(0); assert(movies==6);
    puts("PASS: ordinary teleports do not play the train movie");
    printf("PASS: %d followers and their inventories preserved on all six journeys\n", follower_count);
    return 0;
}
