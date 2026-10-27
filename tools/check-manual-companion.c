#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <tig/tig.h>
#include "game/gamelib.h"
#include "game/gsound.h"
#include "game/highres_config.h"
#include "game/name.h"
#include "ui/gameuilib.h"
#include "../source/src/ui/modern_ui.c"
void sub_549A70(void);

static void render(const char* capture)
{
    button_count=0;SDL_SetRenderDrawColor(renderer,17,13,9,255);SDL_RenderClear(renderer);
    drawing_play_panel=true;draw_play_panels();drawing_play_panel=false;
    if(capture){SDL_Surface* surface=SDL_RenderReadPixels(renderer,NULL);assert(surface&&SDL_SavePNG(surface,capture));SDL_DestroySurface(surface);}
}
static void click(int id)
{
    render(NULL);Button* target=NULL;
    for(int i=0;i<button_count;i++)if(buttons[i].action==id)target=&buttons[i];
    assert(target&&target->enabled);
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_Event event={0};event.type=SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.button=SDL_BUTTON_LEFT;event.button.down=true;
    event.button.x=(target->rect.x+target->rect.w/2)*ww/canvas_w;
    event.button.y=(target->rect.y+target->rect.h/2)*wh/canvas_h;
    int before=stat_base_get(modern_charedit_object(),STAT_UNSPENT_POINTS);
    assert(handle_event(&event));modern_ui_tick();SDL_Delay(150);modern_ui_tick();
    assert(stat_base_get(modern_charedit_object(),STAT_UNSPENT_POINTS)==before);
    event.type=SDL_EVENT_MOUSE_BUTTON_UP;event.button.down=false;
    assert(handle_event(&event));modern_ui_tick();
}
int main(int argc,char** argv)
{
    if((argc!=3&&argc!=4)||chdir(argv[1])||access(".map-export-sandbox",R_OK))return 2;
    setvbuf(stdout,NULL,_IOLBF,0);
    SDL_SetHintWithPriority(SDL_HINT_VIDEO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER,"dummy",SDL_HINT_OVERRIDE);
    SDL_SetHintWithPriority(SDL_HINT_RENDER_DRIVER,"software",SDL_HINT_OVERRIDE);
    highres_config_load();
    TigInitInfo init={0};init.width=argc==4?atoi(argv[3]):1360;init.height=900;init.bpp=32;
    init.texture_width=init.texture_height=2048;init.flags=TIG_INITIALIZE_WINDOWED|TIG_INITIALIZE_NO_SOUND;
    init.art_file_path_resolver=name_resolve_path;init.art_id_reset_func=name_normalize_aid;init.sound_file_path_resolver=gsound_resolve_path;
    assert(tig_init(&init)==TIG_OK);intgame_set_iso_window_width(init.width);intgame_set_iso_window_height(init.height);
    GameInitInfo game={0};assert(intgame_create_iso_window(&game.iso_window_handle)&&gamelib_init(&game));
    sub_549A70();assert(gameuilib_init(&game)&&gamelib_mod_load("Arcanum")&&gameuilib_mod_load());
    gamelib_splash_clear();assert(modern_ui_init());
    printf("Test process %ld, SDL window %u, isolated directory %s\n",(long)getpid(),SDL_GetWindowID(window),argv[1]);
    canvas_w=init.width;canvas_h=900;
    SDL_Texture* target=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_TARGET,init.width,900);
    assert(target&&SDL_SetRenderTarget(renderer,target));SDL_SetRenderLogicalPresentation(renderer,init.width,900,SDL_LOGICAL_PRESENTATION_DISABLED);
    assert(gamelib_load("SlotAuto"));sub_5412D0();in_game=true;
    intgame_mode_set(INTGAME_MODE_MAIN);
    int64_t pc=player_get_local_pc_obj(),companions[8];
    object_flags_unset(pc,OF_OFF);
    assert(modern_followers_get(companions,8)>0);
    int64_t companion=companions[0];
    int pc_points=stat_base_get(pc,STAT_UNSPENT_POINTS),pc_strength=stat_base_get(pc,STAT_STRENGTH);
    auto_level_scheme_set(companion,50);
    stat_base_set(companion,STAT_UNSPENT_POINTS,8);
    stat_base_set(companion,STAT_STRENGTH,12);
    stat_base_set(companion,STAT_DEXTERITY,16);
    stat_base_set(companion,STAT_INTELLIGENCE,16);
    stat_base_set(companion,STAT_WILLPOWER,16);
    assert(charedit_open(companion,CHAREDIT_MODE_PASSIVE));panels_tick();
    assert(!modern_charedit_editable() && !party_has_points(companion));
    modern_charedit_adjust(0,2,1);assert(stat_base_get(companion,STAT_STRENGTH)==12);
    click(654);while(sheet_scheme_page>0)click(655);click(660);
    assert(auto_level_scheme_get(companion)==0 && modern_charedit_editable());
    assert(party_has_points(companion));
    click(651);click(625);
    assert(stat_base_get(companion,STAT_STRENGTH)==13 && stat_base_get(companion,STAT_UNSPENT_POINTS)==7);
    click(624);assert(stat_base_get(companion,STAT_STRENGTH)==12 && stat_base_get(companion,STAT_UNSPENT_POINTS)==8);
    click(624);assert(stat_base_get(companion,STAT_STRENGTH)==12 && stat_base_get(companion,STAT_UNSPENT_POINTS)==8);
    click(625);
    int before=stat_base_get(companion,STAT_UNSPENT_POINTS);
    int skill=BASIC_SKILL_BOW,old_skill=basic_skill_points_get(companion,skill);
    modern_charedit_adjust(1,SKILL_BOW,1);
    assert(basic_skill_points_get(companion,skill)>old_skill && stat_base_get(companion,STAT_UNSPENT_POINTS)==before-1);
    modern_charedit_adjust(1,SKILL_BOW,-1);
    assert(basic_skill_points_get(companion,skill)==old_skill && stat_base_get(companion,STAT_UNSPENT_POINTS)==before);
    modern_charedit_adjust(1,SKILL_BOW,-1);assert(basic_skill_points_get(companion,skill)==old_skill);
    int tech=0;while(tech<TECH_COUNT && tech_degree_get(companion,tech)!=0)tech++;assert(tech<TECH_COUNT);
    modern_charedit_adjust(2,tech,1);
    assert(tech_degree_get(companion,tech)==1 && stat_base_get(companion,STAT_UNSPENT_POINTS)==before-1);
    modern_charedit_adjust(2,tech,-1);assert(tech_degree_get(companion,tech)==0 && stat_base_get(companion,STAT_UNSPENT_POINTS)==before);
    modern_charedit_adjust(2,tech,1);
    int college=0;while(college<COLLEGE_COUNT && spell_college_level_get(companion,college)!=0)college++;assert(college<COLLEGE_COUNT);
    before=stat_base_get(companion,STAT_UNSPENT_POINTS);
    modern_charedit_adjust(3,college,1);
    assert(spell_college_level_get(companion,college)==1 && stat_base_get(companion,STAT_UNSPENT_POINTS)==before-1);
    modern_charedit_adjust(3,college,-1);assert(spell_college_level_get(companion,college)==0 && stat_base_get(companion,STAT_UNSPENT_POINTS)==before);
    modern_charedit_adjust(3,college,1);
    render(argv[2]);click(720);panels_tick();
    assert(charedit_open(companion,CHAREDIT_MODE_PASSIVE));panels_tick();
    before=stat_base_get(companion,STAT_UNSPENT_POINTS);
    modern_charedit_adjust(0,2,-1);assert(stat_base_get(companion,STAT_STRENGTH)==13);
    modern_charedit_adjust(2,tech,-1);assert(tech_degree_get(companion,tech)==1);
    modern_charedit_adjust(3,college,-1);assert(spell_college_level_get(companion,college)==1);
    assert(stat_base_get(companion,STAT_UNSPENT_POINTS)==before);
    stat_base_set(companion,STAT_UNSPENT_POINTS,0);
    modern_charedit_adjust(0,2,1);assert(stat_base_get(companion,STAT_STRENGTH)==13 && !party_has_points(companion));
    charedit_close();panels_tick();
    /* Real level progression keeps its newly awarded points when no scheme is selected. */
    int old_level=stat_base_get(companion,STAT_LEVEL);
    assert(stat_base_get(pc,STAT_UNSPENT_POINTS)==pc_points && stat_base_get(pc,STAT_STRENGTH)==pc_strength);
    stat_base_set(pc,STAT_EXPERIENCE_POINTS,stat_base_get(pc,STAT_EXPERIENCE_POINTS)+level_experience_points_to_next_level(pc));
    level_recalc(pc);
    pc_points=stat_base_get(pc,STAT_UNSPENT_POINTS);pc_strength=stat_base_get(pc,STAT_STRENGTH);
    assert(stat_base_get(companion,STAT_LEVEL)>old_level);
    assert(stat_base_get(companion,STAT_UNSPENT_POINTS)>0 && party_has_points(companion));
    assert(stat_base_get(companion,STAT_STRENGTH)==13);
    hud=true;party_tick();assert(party_visible());
    button_count=0;SDL_SetRenderDrawColor(renderer,17,13,9,255);SDL_RenderClear(renderer);draw_hud();draw_party();
    char capture[1024];snprintf(capture,sizeof(capture),"%s-portraits.png",argv[2]);
    SDL_Surface* pixels=SDL_RenderReadPixels(renderer,NULL);assert(pixels&&SDL_SavePNG(pixels,capture));SDL_DestroySurface(pixels);
    Button* badge=NULL;for(int i=0;i<button_count;i++)if(buttons[i].action==2800)badge=&buttons[i];assert(badge);
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    SDL_Event e={.type=SDL_EVENT_MOUSE_BUTTON_DOWN};e.button.button=SDL_BUTTON_LEFT;e.button.down=true;
    e.button.x=(badge->rect.x+badge->rect.w/2)*ww/canvas_w;e.button.y=(badge->rect.y+badge->rect.h/2)*wh/canvas_h;
    assert(handle_event(&e));modern_ui_tick();SDL_Delay(150);assert(!charedit_is_created());
    e.type=SDL_EVENT_MOUSE_BUTTON_UP;e.button.down=false;assert(handle_event(&e));modern_ui_tick();
    assert(modern_charedit_object()==companion && modern_charedit_editable());
    charedit_close();panels_tick();
    assert(stat_base_get(pc,STAT_UNSPENT_POINTS)==pc_points && stat_base_get(pc,STAT_STRENGTH)==pc_strength);
    assert(gamelib_save("SlotManu","Manual progression test"));
    assert(gamelib_load("SlotManu"));sub_5412D0();pc=player_get_local_pc_obj();object_flags_unset(pc,OF_OFF);
    assert(modern_followers_get(companions,8)>0);companion=companions[0];
    assert(auto_level_scheme_get(companion)==0 && stat_base_get(companion,STAT_STRENGTH)==13);
    assert(tech_degree_get(companion,tech)==1 && spell_college_level_get(companion,college)==1 && party_has_points(companion));
    assert(charedit_open(companion,CHAREDIT_MODE_PASSIVE));panels_tick();
    modern_charedit_scheme_select(1);assert(!modern_charedit_editable() && !party_has_points(companion));
    before=stat_base_get(companion,STAT_UNSPENT_POINTS);modern_charedit_adjust(0,2,1);
    assert(stat_base_get(companion,STAT_UNSPENT_POINTS)==before && stat_base_get(companion,STAT_STRENGTH)==13);
    charedit_close();
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));assert(modern_charedit_editable());charedit_close();
    puts("PASS: manual companion stats/skills/tech/spells, accepted baselines, empty points, real level-up, portrait badge with held click, player isolation, scheme switching and save/load");
    return 0;
}
