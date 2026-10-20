#include <stdio.h>
#undef NDEBUG
#include <assert.h>
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
#include "../source/src/game/dialog.c"

int main(int argc,char** argv)
{
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

    int64_t pc=player_get_local_pc_obj(),allies[8];
    object_flags_unset(pc,OF_OFF);
    int count=modern_followers_get(allies,8);
    assert(count>=3);

    int64_t actor=allies[0];
    assert(modern_party_select(actor));
    assert(combat_input_actor()==actor && player_get_local_pc_obj()==pc);
    assert(inven_ui_open(actor,OBJ_HANDLE_NULL,INVEN_UI_MODE_INVENTORY));
    ModernInventory inventory;
    assert(modern_inventory_state(&inventory) && inventory.pc==actor);
    inven_ui_destroy();
    sub_579FA0(actor,1);
    assert(intgame_mode_get()==INTGAME_MODE_SKILL);
    TargetDescriptor target;target_descriptor_set_obj(&target,allies[1]);
    skill_ui_apply(&target);
    assert(inven_ui_is_created());
    assert(modern_inventory_state(&inventory) && inventory.pc==actor);
    assert(intgame_mode_get()==INTGAME_MODE_STEAL);
    inven_ui_destroy();
    sub_579FA0(actor,0);assert(critter_is_concealed(actor));
    sub_579FA0(actor,0);assert(!critter_is_concealed(actor));
    assert(spell_is_known(actor,SPELL_MINOR_HEALING));
    assert(spell_ui_activate(actor,SPELL_MINOR_HEALING)==SPELL_UI_ACTIVATE_OK);
    assert(intgame_mode_get()==INTGAME_MODE_SPELL);
    spell_ui_cancel();
    int old_charisma=stat_base_get(pc,STAT_CHARISMA);
    int actor_charisma=stat_base_get(actor,STAT_CHARISMA);
    stat_base_set(pc,STAT_CHARISMA,2);stat_base_set(actor,STAT_CHARISMA,18);
    DialogState state={0};state.pc_obj=pc;state.npc_obj=allies[1];
    dialog_speaker_set(actor);
    assert(sub_4150D0(&state,"ch15"));
    assert(state.pc_obj==pc && player_get_local_pc_obj()==pc);
    dialog_speaker_set(OBJ_HANDLE_NULL);
    assert(!sub_4150D0(&state,"ch15"));
    stat_base_set(pc,STAT_CHARISMA,old_charisma);stat_base_set(actor,STAT_CHARISMA,actor_charisma);
    dialog_speaker_set(actor);
    int quest=1;
    char condition[64];snprintf(condition,sizeof(condition),"qu%d %d",quest,quest_state_get(pc,quest));
    assert(sub_4150D0(&state,condition));
    int hero_skill=obj_arrayfield_int32_get(pc,OBJ_F_CRITTER_BASIC_SKILL_IDX,BASIC_SKILL_HAGGLE);
    int actor_skill=obj_arrayfield_int32_get(actor,OBJ_F_CRITTER_BASIC_SKILL_IDX,BASIC_SKILL_HAGGLE);
    obj_arrayfield_int32_set(actor,OBJ_F_CRITTER_BASIC_SKILL_IDX,BASIC_SKILL_HAGGLE,20);
    state.field_17F0[1]=state.field_1804[1]=0;
    dialog_perform_training(BASIC_SKILL_HAGGLE,&state);
    assert(basic_skill_training_get(actor,BASIC_SKILL_HAGGLE)==TRAINING_APPRENTICE);
    assert(obj_arrayfield_int32_get(pc,OBJ_F_CRITTER_BASIC_SKILL_IDX,BASIC_SKILL_HAGGLE)==hero_skill);
    obj_arrayfield_int32_set(actor,OBJ_F_CRITTER_BASIC_SKILL_IDX,BASIC_SKILL_HAGGLE,actor_skill);
    dialog_speaker_set(OBJ_HANDLE_NULL);
    sub_460410(actor,allies[1]);
    const DialogState* current=modern_dialog_state();
    assert(current && current->pc_obj==pc && dialog_speaker_get(pc)==actor);
    puts("PASS: native companion inventory, pickpocket targeting, sneaking, spell targeting, dialogue charisma, companion training and hero quest state");
    return 0;
}
