/* Run after GUI --authoring and --project-playtest: check-worlded-authoring COPY/game SLOT.
 * This harness refuses any directory without the test-copy marker. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/obj.h"
#include "game/object.h"
#include "game/location.h"
#include "game/sector.h"
#include "game/sector_object_list.h"
#include "game/map.h"
#include "game/teleport.h"
#include "game/obj_pool.h"
#include "game/item.h"
#include "game/portal.h"
#include "game/path.h"
#include "game/script.h"
#include "game/script_name.h"
#include "game/dialog.h"
#include "ui/iso.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#define REQUIRE(x) do {if(!(x)){fprintf(stderr,"Failed: %s (line %d)\n",#x,__LINE__);return 1;}} while(0)
static void render(void) {
    for(int i=0;i<3;i++) {modern_ui_tick();tig_ping();gamelib_ping();iso_invalidate_rect(NULL);iso_redraw();tig_window_display();TigMessage msg;while(tig_message_dequeue(&msg)==TIG_OK){}}
}
int main(int argc,char** argv) {
    setvbuf(stdout,NULL,_IOLBF,0);
    if(argc!=3 || chdir(argv[1]) || access(".worlded-playtest",R_OK)) return 2;
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    puts("Initialize TIG");
    highres_config_load();TigInitInfo init={0};init.width=highres_config_get()->width;init.height=highres_config_get()->height;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    REQUIRE(tig_init(&init)==TIG_OK);puts("Initialize game");intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};game.draw_func=iso_redraw;game.invalidate_rect_func=iso_invalidate_rect;
    REQUIRE(intgame_create_iso_window(&game.iso_window_handle));REQUIRE(gamelib_init(&game));puts("Initialize UI");REQUIRE(gameuilib_init(&game));puts("Load module");
    REQUIRE(gamelib_mod_load("Arcanum"));REQUIRE(gameuilib_mod_load());gamelib_splash_clear();REQUIRE(modern_ui_init());
    puts("Load copied save");REQUIRE(gamelib_load(argv[2]));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    char path[260];
    REQUIRE(script_name_build_scr_name(49000,path,sizeof(path)));
    REQUIRE(strstr(path,"49000_authoring.scr")!=NULL);
    Script script={0};script.num=49000;
    ScriptInvocation invocation={0};invocation.script=&script;
    invocation.triggerer_obj=invocation.attachee_obj=player_get_local_pc_obj();
    invocation.attachment_point=SAP_USE;
    script_global_var_set(1900,-1);script_execute(&invocation);
    REQUIRE(script_global_var_get(1900)==8);
    REQUIRE(script_name_build_dlg_name(49001,path,sizeof(path)));
    int dlg;REQUIRE(dialog_load(path,&dlg));dialog_unload(dlg);
    puts("PASS: new script indexed and executed (for/if/continue = 8); companion DLG loaded by native engine");
    gameuilib_exit();gamelib_exit();tig_exit();return 0;
}
