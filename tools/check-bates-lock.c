#include <stdio.h>
#undef NDEBUG
#include <assert.h>
#include "game/script.h"
#include "game/quest.h"
#include <unistd.h>
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
#include "ui/modern_ui.h"
#include "ui/wmap_ui.h"
#include "../source/src/ui/modern_ui.c"

int main(int argc,char** argv)
{
    if(argc!=4 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
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
    if(!modern_ui_init() || !gamelib_load(argv[2]))return 6;
    intgame_mode_set(INTGAME_MODE_MAIN);

    int64_t pc=player_get_local_pc_obj(), guard=OBJ_HANDLE_NULL, gate=OBJ_HANDLE_NULL;
    assert(quest_state_get(pc,1040)==4 && script_global_flag_get(1024)==1);
    ObjectList objects;object_list_vicinity(pc,OBJ_TM_PORTAL|OBJ_TM_NPC,&objects);
    for(ObjectNode* node=objects.head;node;node=node->next) {
        int64_t obj=node->obj;
        if(obj_field_int32_get(obj,OBJ_F_TYPE)==OBJ_TYPE_PORTAL
            && obj_field_int32_get(obj,OBJ_F_NAME)==806) {
            assert(!gate);gate=obj;
        }
        Script scr={0};obj_arrayfield_script_get(obj,OBJ_F_SCRIPTS_IDX,SAP_DIALOG,&scr);
        if(scr.num==1060){assert(!guard);guard=obj;}
    }
    object_list_destroy(&objects);
    assert(gate && guard);
    printf("Bates before original unlock script: locked=%d\n",object_locked_get(gate));
    object_script_execute(pc,guard,OBJ_HANDLE_NULL,SAP_DIALOG,62);
    assert(!object_locked_get(gate));
    assert(obj_field_int32_get(gate,OBJ_F_PORTAL_FLAGS)&OPF_NEVER_LOCKED);
    TimeEvent relock={0};relock.params[0].object_value=gate;
    assert(object_lock_timeevent_process(&relock));
    assert(!object_locked_get(gate));
    assert(quest_state_get(pc,1040)==4 && script_global_flag_get(1024)==1);
    if(strcmp(argv[3],"-")) {
        assert(gamelib_save(argv[3],"Porte Bastien corrigee"));
        assert(gamelib_load(argv[3]));
        pc=player_get_local_pc_obj();gate=OBJ_HANDLE_NULL;
        object_list_vicinity(pc,OBJ_TM_PORTAL,&objects);
        for(ObjectNode* node=objects.head;node;node=node->next) {
            if(obj_field_int32_get(node->obj,OBJ_F_NAME)==806)gate=node->obj;
        }
        assert(gate && !object_locked_get(gate));
        assert(obj_field_int32_get(gate,OBJ_F_PORTAL_FLAGS)&OPF_NEVER_LOCKED);
        object_list_destroy(&objects);
        assert(quest_state_get(pc,1040)==4 && script_global_flag_get(1024)==1);
        puts("PASS repaired COPY saved and reloaded with access and quest unchanged");
    }
    puts("PASS original Bates script permanently unlocks gate; pending relock ignored");
    return 0;
}
