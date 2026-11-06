#include "game/map.h"

/* Tutorial resources are optional; ordinary new games retain the campaign start. */
enum TutorialStep {
    TUTORIAL_IDLE, TUTORIAL_GUIDE, TUTORIAL_MOVE, TUTORIAL_CAMERA, TUTORIAL_BAG,
    TUTORIAL_SEARCH, TUTORIAL_FILTER, TUTORIAL_DETAILS, TUTORIAL_COMPARE, TUTORIAL_EQUIP,
    TUTORIAL_TRANSFER, TUTORIAL_HOTBAR, TUTORIAL_BANK, TUTORIAL_DETACH, TUTORIAL_POSITION,
    TUTORIAL_REGROUP, TUTORIAL_MAP, TUTORIAL_ZOOM, TUTORIAL_NOTE, TUTORIAL_ROUTE,
    TUTORIAL_JOURNAL, TUTORIAL_JOURNAL_FILTER, TUTORIAL_TRACK, TUTORIAL_SHEET,
    TUTORIAL_SPELLS, TUTORIAL_SKILLS, TUTORIAL_SCHEMATICS, TUTORIAL_REST,
    TUTORIAL_SERVICES, TUTORIAL_FIGHT, TUTORIAL_TURN, TUTORIAL_AI, TUTORIAL_LOG, TUTORIAL_DONE, TUTORIAL_HELLO,
    /* Append new lessons to preserve the guide script's existing step IDs. */
    TUTORIAL_PORTRAIT
};
static enum TutorialStep tutorial_step;
static bool tutorial_checked, tutorial_invite, tutorial_starting;
static int tutorial_last_menu = -1;
static int64_t tutorial_origin;
static bool tutorial_departure, tutorial_skip, tutorial_new_session;
static SDL_FRect tutorial_next_rect;
static bool tutorial_pause_seen,tutorial_route_started;
static tig_sound_handle_t tutorial_voice=TIG_SOUND_HANDLE_INVALID;
static const char* tutorial_voice_id;
static Uint64 tutorial_hello_started;
static mes_file_handle_t tutorial_journal_fr=MES_FILE_HANDLE_INVALID, tutorial_journal_en=MES_FILE_HANDLE_INVALID;
static bool tutorial_pack_loaded;

static void tutorial_voice_stop(void)
{
    if(tutorial_voice!=TIG_SOUND_HANDLE_INVALID) {
        tig_sound_destroy(tutorial_voice);
        tutorial_voice=TIG_SOUND_HANDLE_INVALID;
    }
    tutorial_voice_id=NULL;
}

static void tutorial_exit(void)
{
    tutorial_voice_stop();
    if(tutorial_journal_fr!=MES_FILE_HANDLE_INVALID){mes_unload(tutorial_journal_fr);tutorial_journal_fr=MES_FILE_HANDLE_INVALID;}
    if(tutorial_journal_en!=MES_FILE_HANDLE_INVALID){mes_unload(tutorial_journal_en);tutorial_journal_en=MES_FILE_HANDLE_INVALID;}
}

void modern_tutorial_load(void)
{
    SDL_PathInfo info;
    if(!SDL_GetPathInfo("ArcanumModern/tutorial/base/tutorial/session-v1",&info))return;
    if(!tig_file_repository_add("ArcanumModern/tutorial/base"))return;
    if(!mes_load("tutorial/fr/journal.mes",&tutorial_journal_fr))return;
    if(!mes_load("tutorial/en/journal.mes",&tutorial_journal_en)){
        mes_unload(tutorial_journal_fr);
        tutorial_journal_fr=MES_FILE_HANDLE_INVALID;
        return;
    }
    tutorial_pack_loaded=true;
}

const char* modern_tutorial_dialog_path(const char* path)
{
    if(!tutorial_pack_loaded || !path)return NULL;
    if(SDL_strcasecmp(path,"dlg\\48874_guide.dlg")!=0
        && SDL_strcasecmp(path,"dlg/48874_guide.dlg")!=0)return NULL;
    return tr("tutorial\\fr\\48874_guide.dlg","tutorial\\en\\48874_guide.dlg");
}

static const char* tutorial_journal_lookup(int num)
{
    if(!tutorial_pack_loaded)return NULL;
    mes_file_handle_t handle=tr("fr","en")[0]=='f'?tutorial_journal_fr:tutorial_journal_en;
    MesFileEntry entry={.num=num};
    return handle!=MES_FILE_HANDLE_INVALID && mes_search(handle,&entry)?entry.str:NULL;
}

const char* modern_tutorial_quest_text(int num)
{
    return num>=1980 && num<=1983?tutorial_journal_lookup(num):NULL;
}

const char* modern_tutorial_rumor_text(int rumor)
{
    return rumor>=2980 && rumor<=2983?tutorial_journal_lookup(rumor):NULL;
}

static bool tutorial_save_marker(const char* name)
{
    SDL_CreateDirectory("modern-ui");
    SDL_CreateDirectory("modern-ui/tutorial");
    char path[128];
    snprintf(path,sizeof(path),"modern-ui/tutorial/%s",name);
    return SDL_SaveFile(path,"1",1);
}

static void tutorial_voice_play(const char* id)
{
    if(tutorial_voice_id && strcmp(tutorial_voice_id,id)==0)return;
    tutorial_voice_stop();
    tutorial_voice_id=id;
    char path[256];
    // The same English recordings accompany both French and English subtitles.
    snprintf(path,sizeof(path),"sound/tutorial/%s.mp3",id);
    if(tig_file_exists(path,NULL)) {
        tutorial_voice=gsound_play_voice(path,0);
        SDL_Log("Tutorial voice: %s, handle=%d, tick=%llu",id,tutorial_voice,(unsigned long long)SDL_GetTicks());
    }
    else SDL_Log("Tutorial voice missing: %s",path);
}

bool modern_tutorial_skip_movie(const char* path)
{
    if (!tutorial_departure || !path) return false;
    const char* name=path;
    for (const char* c=path; *c; ++c) if (*c=='/' || *c=='\\') name=c+1;
    /* Arcanum module movies 1 and 7, played by its starting teleport. */
    return SDL_strcasecmp(name,"00069.bik")==0 || SDL_strcasecmp(name,"01138.bik")==0;
}

static void tutorial_check(void)
{
    if (tutorial_checked) return;
    tutorial_checked = true;
    SDL_PathInfo info;
    tutorial_available = tutorial_pack_loaded;
    tutorial_invite = tutorial_available && !SDL_GetPathInfo("modern-ui/tutorial/completed-v1", &info)
        && !SDL_GetPathInfo("modern-ui/tutorial/dismissed-v1", &info);
}

bool modern_tutorial_active(void)
{
    if(tutorial_starting || tutorial_new_session)return true;
    if(!tutorial_available || gamelib_in_reset() || gamelib_in_load())return false;
    ObjectID id;
    if(!objid_id_from_str(&id,"G_9446D492_810B_D640_A404_25CBFAC90F01"))return false;
    int64_t guide=obj_pool_perm_lookup(id);
    if(!guide)return false;
    int step=script_local_counter_get(guide,SAP_DIALOG,0)&255;
    return step>=TUTORIAL_GUIDE && step<=TUTORIAL_PORTRAIT;
}

static void tutorial_draw_invite(void)
{
    draw_menu_background();
    float w=fminf(canvas_w-32,760), x=(canvas_w-w)/2;
    const char* description=tr("Découvrez les spécificités d’Arcanum Renaissance par rapport au jeu de base. Rencontrez le garde et apprenez à voyager et à combattre ensemble.",
        "Discover what makes Arcanum Renaissance different from the base game. Meet the guard and learn to travel and fight together.");
    int rows=menu_paragraph(description,(SDL_FRect){0,0,w-48,0},0,GOLD);
    float text_height=rows*text_line_height(), height=260+text_height, y=(canvas_h-height)/2;
    menu_plaque((SDL_FRect){x,y,w,height},true,false);
    centered(2,tr("Bienvenue sur l’île","Welcome to the island"),canvas_w/2,y+32,GOLD);
    menu_paragraph(description,(SDL_FRect){x+24,y+85,w-48,text_height},0,GOLD);
    menu_button((SDL_FRect){x+24,y+105+text_height,w-48,54},680,tr("Commencer le tutoriel","Start tutorial"),true);
    float half=(w-60)/2;
    menu_button((SDL_FRect){x+24,y+177+text_height,half,48},681,tr("Plus tard","Later"),true);
    menu_button((SDL_FRect){x+36+half,y+177+text_height,half,48},682,tr("Ne plus proposer","Don’t ask again"),true);
}

// Preserve the module's normal start and override it only for a tutorial launch.
static void tutorial_start_location(bool island)
{
    static int64_t campaign_x,campaign_y;
    static bool overridden;
    int map=map_by_type(MAP_TYPE_START_MAP);
    char* name=NULL;
    int worldmap,area;
    if(!map || !map_get_name(map,&name) || !name
        || SDL_strcasecmp(name,"Arcanum1-024-fixed")!=0)return;
    if(island && !overridden){
        if(!map_get_starting_location(map,&campaign_x,&campaign_y))return;
        overridden=true;
    }
    if(!overridden)return;
    map_get_worldmap(map,&worldmap);
    map_get_area(map,&area);
    char map_name[256];SDL_strlcpy(map_name,name,sizeof(map_name));
    map_list_info_set(map-1,map_name,island?25621:campaign_x,island?50332:campaign_y,true,worldmap,area);
    if(!island)overridden=false;
}

static void tutorial_begin(void)
{
    if (!tutorial_pack_loaded) return;
    tutorial_voice_stop();
    tutorial_invite=false;
    tutorial_starting=true;
    tutorial_new_session=true;
    tutorial_departure=true;
    tutorial_skip=false;
    tutorial_pause_seen=tutorial_route_started=false;
    tutorial_last_menu=-1;
    tutorial_step=TUTORIAL_GUIDE;
}

static bool tutorial_menu_tick(int type)
{
    tutorial_check();
    if (!tutorial_available) return false;
    tutorial_voice_stop();
    if(tutorial_step==TUTORIAL_SERVICES && (type==MM_WINDOW_MAINMENU_IN_PLAY || type==MM_WINDOW_MAINMENU_IN_PLAY_LOCKED))tutorial_pause_seen=true;
    if (type==MM_WINDOW_MAINMENU && tutorial_invite) {
        int choice=pending_action;
        pending_action=-1;
        if (choice==680) tutorial_begin();
        if (choice==681 || choice==682) {
            if (choice==682 && !tutorial_save_marker("dismissed-v1")) return true;
            tutorial_invite=false;
        }
        return true;
    }
    if (type==MM_WINDOW_SINGLE_PLAYER && pending_action==680) {
        pending_action=-1;
        tutorial_begin();
    }
    if (!tutorial_starting) {
        if (type==MM_WINDOW_MAINMENU || type==MM_WINDOW_SINGLE_PLAYER) {
            tutorial_start_location(false);
            tutorial_departure=false;
            tutorial_new_session=false;
            tutorial_step=TUTORIAL_IDLE;
        }
        return false;
    }
    if (type==tutorial_last_menu) return true;
    tutorial_last_menu=type;
    switch(type) {
    case MM_WINDOW_MAINMENU:
    case MM_WINDOW_SINGLE_PLAYER:
    case MM_WINDOW_PICK_NEW_OR_PREGEN:
    case MM_WINDOW_PREGEN_CHAR:
        if (type==MM_WINDOW_PREGEN_CHAR) {
            tutorial_start_location(true);
            tutorial_starting=false;
        }
        modern_mainmenu_activate(0);
        return true;
    default:
        tutorial_starting=false;
        tutorial_step=TUTORIAL_IDLE;
        return false;
    }
}
