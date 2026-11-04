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
#include "../source/src/ui/wmap_ui.c"

static void travel(int64_t actor, int map, int64_t location)
{
    TeleportData jump={.obj=actor,.map=map,.loc=location};
    assert(teleport_do(&jump));
    teleport_ping(0);
}

static int64_t check_follower(ObjectID id)
{
    int64_t hero=player_get_local_pc_obj();
    int64_t follower=obj_pool_perm_lookup(id);
    assert(obj_handle_is_valid(follower));
    assert((obj_field_int32_get(follower,OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT|OF_OFF))==0);
    assert(critter_leader_get(follower)==hero);
    int matches=0;
    for(int i=0;i<obj_arrayfield_length_get(hero,OBJ_F_CRITTER_FOLLOWER_IDX);i++)
        if(obj_arrayfield_handle_get(hero,OBJ_F_CRITTER_FOLLOWER_IDX,i)==follower)matches++;
    assert(matches==1);
    int iter;int64_t obj;int copies=0;
    if(obj_pool_walk_first(&obj,&iter))do {
        if(objid_is_equal(obj_get_id(obj),id))copies++;
    }while(obj_pool_walk_next(&obj,&iter));
    assert(copies==1);
    return follower;
}


static void check_party(ObjectID* ids,int* inventory,int count,const char* stage)
{
    for(int i=0;i<count;i++) {
        int64_t follower=check_follower(ids[i]);
        assert(obj_field_int32_get(follower,OBJ_F_CRITTER_INVENTORY_NUM)==inventory[i]);
    }
    assert(!sector_save_failed());
    printf("PASS %s: map %d, %d followers and inventories intact\n",stage,map_current_map(),count);
}

static bool lookup_entrance(int source_map,int target_map,JumpPoint* result)
{
    char* name;char path[512];assert(map_get_name(source_map,&name));
    snprintf(path,sizeof(path),"maps\\%s\\map.jmp",name);
    TigFile* f=tig_file_fopen(path,"rb");assert(f);
    int count;assert(tig_file_fread(&count,sizeof(count),1,f)==1);
    JumpPoint jump={0};bool found=false;
    for(int i=0;i<count;i++) {
        assert(tig_file_fread(&jump,sizeof(jump),1,f)==1);
        if(jump.dst_map==target_map){found=true;break;}
    }
    tig_file_fclose(f);if(found)*result=jump;return found;
}

static JumpPoint find_entrance(int source_map,int target_map)
{
    JumpPoint jump;
    if(lookup_entrance(source_map,target_map,&jump))return jump;
    /* Scripted doors use TeleportData too; recover their landing tile from the exit. */
    if(lookup_entrance(target_map,source_map,&jump))
        return (JumpPoint){.loc=jump.dst_loc,.dst_map=target_map,.dst_loc=jump.loc};
    int64_t x,y;assert(map_get_starting_location(target_map,&x,&y));
    return (JumpPoint){.loc=obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION),
        .dst_map=target_map,.dst_loc=location_make(x,y)};
}

/* Exercise the real Teleport destination selection, fatigue charge and sector flush. */
static void cast_teleport(int area,bool exhausted)
{
    int64_t hero=player_get_local_pc_obj();
    object_flags_unset(hero,OF_OFF);
    critter_fatigue_damage_set(hero,exhausted?critter_fatigue_max(hero)-20:0);
    assert(area_set_known(hero,area));
    wmap_ui_select(hero,SPELL_TELEPORTATION);
    assert(wmap_ui_created && wmap_ui_selecting && wmap_ui_mode==WMAP_UI_MODE_WORLD);
    int64_t destination=area_get_location(area);
    WmapCoords center;sub_561490(destination,&center);sub_563590(&center,false);
    WmapInfo* info=&wmap_ui_mode_info[wmap_ui_mode];
    int px=-1,py=-1;
    for(int y=info->field_14.y; y<info->field_14.y+info->field_14.height && px<0; y+=2)
        for(int x=info->field_14.x; x<info->field_14.x+info->field_14.width; x+=2) {
            WmapCoords coords;int64_t loc;info->field_2BC(x,y,&coords);sub_561800(&coords,&loc);
            if(area_get_nearest_known_area(loc,hero,qword_66D850)==area){px=x;py=y;break;}
        }
    assert(px>=0);
    TigRect origin={0,0,800,600};hrp_apply(&origin,GRAVITY_CENTER_HORIZONTAL|GRAVITY_CENTER_VERTICAL);
    TigMessage message={0};message.type=TIG_MESSAGE_MOUSE;
    message.data.mouse.event=TIG_MESSAGE_MOUSE_LEFT_BUTTON_UP;
    message.data.mouse.x=px+origin.x;message.data.mouse.y=py+origin.y;
    assert(wmap_ui_message_filter(&message));
    assert(!wmap_ui_created);
    teleport_ping(0);
    assert(map_current_map()==map_by_type(MAP_TYPE_START_MAP));
    hero=player_get_local_pc_obj();
    assert(location_dist(obj_field_int64_get(hero,OBJ_F_LOCATION),destination)<4);
    printf("Teleport area %d: fatigue %d/%d, unconscious %d\n",area,critter_fatigue_damage_get(hero),critter_fatigue_max(hero),critter_is_unconscious(hero));
    if(exhausted)assert(critter_is_unconscious(hero));
    critter_fatigue_damage_set(hero,0);
    sub_423FF0(hero);
}

int main(int argc,char** argv)
{
    setvbuf(stdout,NULL,_IONBF,0);
    if(argc!=3 || chdir(argv[1]) || access(".map-export-sandbox",R_OK))return 2;
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
    sub_5412D0();
    intgame_mode_set(INTGAME_MODE_MAIN);
    assert(!sub_45B300());



    /* Frozen test save is copied from the installation, never opened in place. */
    int areas[]={21,21,6,2,21};
    int maps[]={12,3,19,79,73};
    for(int exhausted=0;exhausted<=1;exhausted++)for(int test=0;test<5;test++) {
        assert(gamelib_load(argv[2]));
        int64_t hero=player_get_local_pc_obj();object_flags_unset(hero,OF_OFF);
        int64_t allies[16];int n=modern_followers_get(allies,16),count=0;
        ObjectID ids[16];int inventory[16];
        for(int i=0;i<n;i++)if(allies[i]!=hero){ids[count]=obj_get_id(allies[i]);inventory[count++]=obj_field_int32_get(allies[i],OBJ_F_CRITTER_INVENTORY_NUM);}
        assert(count>=2);
        cast_teleport(areas[test],exhausted);
        check_party(ids,inventory,count,"after Teleport");
        if(maps[test]==73) {
            int64_t x,y;assert(map_get_starting_location(8,&x,&y));
            travel(player_get_local_pc_obj(),8,location_make(x,y));
            check_party(ids,inventory,count,"after warp to Void");
        }
        int source_map=map_current_map();
        JumpPoint entrance=find_entrance(source_map,maps[test]);
        travel(player_get_local_pc_obj(),entrance.dst_map,entrance.dst_loc);
        assert(map_current_map()==maps[test]);
        check_party(ids,inventory,count,"after entrance");
        if(maps[test]==12 || maps[test]==3) {
            JumpPoint stairs=find_entrance(maps[test],maps[test]+1);
            travel(player_get_local_pc_obj(),stairs.dst_map,stairs.dst_loc);
            assert(map_current_map()==maps[test]+1);
            check_party(ids,inventory,count,"after stairs");
            travel(player_get_local_pc_obj(),maps[test],stairs.loc);
        }
        travel(player_get_local_pc_obj(),source_map,entrance.loc);
        check_party(ids,inventory,count,"after exit");
        assert(gamelib_save("Slot0095","Teleport cave regression"));
        assert(gamelib_load("Slot0095"));
        check_party(ids,inventory,count,"after save/load");
    }
    puts("PASS: Teleport, entrance, stairs, exit, save/load with normal and exhausted caster");
    return 0;
}
