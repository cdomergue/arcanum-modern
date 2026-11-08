/* Linux regression: use a deployable arachnid from a controlled companion's
 * first hotkey. Requires a disposable copy; memory growth is locally capped. */
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "game/player.h"
#include "game/critter.h"
#include "game/combat.h"
#include "game/anim.h"
#include "game/descriptions.h"
#include "game/item.h"
#include "game/location.h"
#include "game/map.h"
#include "game/obj.h"
#include "game/obj_pool.h"
#include "game/object.h"
#include "game/proto.h"
#include "game/timeevent.h"
#include "game/teleport.h"
#include "ui/gameuilib.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/iso.h"
#include "ui/modern_ui.h"
#include "ui/follower_ui.h"
#include "ui/hotkey_ui.h"
#include "ui/inven_ui.h"

static void cap_memory(void)
{
    FILE* file=fopen("/proc/self/statm","r");unsigned long pages;
    assert(file && fscanf(file,"%lu",&pages)==1);fclose(file);
    struct rlimit limit;assert(getrlimit(RLIMIT_AS,&limit)==0);
    rlim_t maximum=(rlim_t)pages*sysconf(_SC_PAGESIZE)+128*1024*1024;
    if(limit.rlim_cur==RLIM_INFINITY || maximum<limit.rlim_cur)limit.rlim_cur=maximum;
    assert(setrlimit(RLIMIT_AS,&limit)==0);
    puts("Native arachnid test memory cap: current virtual size + 128 MiB");
}

static int collect(int64_t pc,int64_t wanted)
{
    ObjectList list;object_list_all_followers(pc,&list);int count=0,matches=0;
    for(ObjectNode* node=list.head;node;node=node->next){count++;assert(count<256);matches+=node->obj==wanted;}
    object_list_destroy(&list);assert(matches==(wanted!=0));return count;
}

int main(int argc,char** argv)
{
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    highres_config_load();TigInitInfo init={0};init.width=1280;init.height=720;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle));
    assert(gamelib_init(&game) && gameuilib_init(&game));
    assert(gamelib_mod_load("Arcanum") && gameuilib_mod_load());gamelib_splash_clear();
    assert(modern_ui_init() && gamelib_load(argv[2]));sub_5412D0();intgame_mode_set(INTGAME_MODE_MAIN);
    int windows_count=0;SDL_Window** windows=SDL_GetWindows(&windows_count);assert(windows && windows_count>0);
    printf("Native arachnid test PID %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(windows[0]),argv[1]);SDL_free(windows);
    int64_t pc=player_get_local_pc_obj(),allies[128],companion=0;
    object_flags_unset(pc,OF_OFF);int count=modern_followers_get(allies,128);
    for(int i=0;i<count;i++)if(critter_is_active(allies[i])){companion=allies[i];break;}
    assert(companion && !combat_turn_based_is_active());
    int64_t origin=obj_field_int64_get(pc,OBJ_F_LOCATION),near;
    assert(location_in_dir(origin,3,&near));sub_43E770(companion,near,0,0);
    assert(modern_party_select(companion) && combat_input_actor()==companion && combat_input_ready(companion));
    cap_memory();
    int before=collect(pc,0);
    Hotkey hero=*sub_57F240(0);
    int64_t device;assert(object_create(sub_4685A0(BP_MECHANIZED_ARACHNID),near,&device));assert(item_transfer(device,companion));
    int followers=obj_arrayfield_length_get(companion,OBJ_F_CRITTER_FOLLOWER_IDX);
    assert(modern_hotkey_assign(0,HOTKEY_ITEM,0,device));
    TigMessage key={.type=TIG_MESSAGE_KEYBOARD};key.data.keyboard.scancode=SDL_SCANCODE_1;
    intgame_process_event(&key);assert(inven_ui_drag_item_obj_get()==device);
    puts("PASS: controlled companion assigns and activates slot 1 with the real item");
    int64_t target=near;assert(location_in_dir(target,3,&target));assert(location_in_dir(target,3,&target));
    sub_573840();assert(anim_goal_throw_item(companion,device,target));
    for(int tick=0;tick<1500 && obj_arrayfield_length_get(companion,OBJ_F_CRITTER_FOLLOWER_IDX)==followers;tick++){
        tig_ping();gamelib_ping();modern_ui_tick();SDL_Delay(10);
    }
    assert(obj_arrayfield_length_get(companion,OBJ_F_CRITTER_FOLLOWER_IDX)==followers+1);
    int64_t bot=obj_arrayfield_handle_get(companion,OBJ_F_CRITTER_FOLLOWER_IDX,followers);
    assert(bot && sub_49B290(bot)==BP_MECHANIZED_ARACHNID_2);
    assert(critter_leader_get(bot)==companion && critter_pc_leader_get(bot)==pc);
    assert(collect(pc,bot)==before+1);
    Hotkey* used=modern_hotkey_get(0);assert(!used || used->type==HOTKEY_NONE);
    assert(sub_57F240(0)->type==hero.type && sub_57F240(0)->data==hero.data);
    for(int i=0;i<500;i++)assert(collect(pc,bot)==before+1);
    puts("PASS: native throw script creates exactly one nested follower; repeated party traversal stays bounded and the used slot clears");
    ObjectID companion_id=obj_get_id(companion),bot_id=obj_get_id(bot);
    assert(gamelib_save("SlotArac","Isolated nested arachnid regression"));assert(gamelib_load("SlotArac"));
    pc=player_get_local_pc_obj();companion=obj_pool_perm_lookup(companion_id);bot=obj_pool_perm_lookup(bot_id);
    assert(companion && bot && critter_leader_get(bot)==companion && critter_pc_leader_get(bot)==pc);
    assert(collect(pc,bot)==before+1);
    puts("PASS: save/load preserves the companion's arachnid without duplicates");
    gameuilib_exit();gamelib_exit();tig_exit();return 0;
}
