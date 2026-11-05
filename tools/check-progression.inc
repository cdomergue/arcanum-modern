/* Native gameplay checks, included only by the marked disposable-game harness. */
#include "game/modern_progression.h"
#include "game/modern_world_updates.h"
#include "game/obj_pool.h"

static void progression_settle(void)
{
    SDL_Delay(550);modern_progression_tick();
}

static void check_progression(void)
{
    settings_set_value(&settings,MODERN_AUTOSAVE_KEY,0);
    settings_set_value(&settings,DIFFICULTY_KEY,1);
    modern_world_updates_tick();
    assert(modern_world_updates_save());
    modern_progression_reset();
    assert(!modern_encounter_grace_remaining());
    modern_encounter_started();
    modern_encounter_advance(60000);assert(modern_encounter_grace_remaining()==5000);
    modern_encounter_travel(true);modern_encounter_advance(4900);
    assert(modern_encounter_grace_remaining()==100);
    sub_45B320();modern_encounter_advance(1000);sub_45B340();
    assert(modern_encounter_grace_remaining()==100);
    assert(modern_progression_save());modern_progression_reset();
    assert(modern_progression_load() && modern_encounter_grace_remaining()==100);
    modern_encounter_advance(10000);assert(modern_encounter_grace_remaining()==100);
    modern_encounter_travel(true);modern_encounter_advance(99);
    assert(modern_encounter_grace_remaining()==1);
    modern_encounter_advance(1);assert(!modern_encounter_grace_remaining());
    modern_encounter_travel(false);
    puts("PASS: five seconds of active travel, pause/menus and save/load preserve grace");

    int64_t pc=player_get_local_pc_obj(),followers[8],veteran,enemy;
    assert(modern_followers_get(followers,8)==1);
    int64_t junior=followers[0];
    assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(32,30),&veteran));
    assert(critter_follow(veteran,pc,true));
    auto_level_scheme_set(pc,0);auto_level_scheme_set(junior,0);auto_level_scheme_set(veteran,0);
    level_set(pc,1);level_set(junior,1);level_set(veteran,5);
    /* Simulate a legacy recruited NPC with no stored XP. */
    obj_arrayfield_int32_set(veteran,OBJ_F_CRITTER_STAT_BASE_IDX,STAT_EXPERIENCE_POINTS,0);
    int junior_points=stat_base_get(junior,STAT_UNSPENT_POINTS);
    assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(33,30),&enemy));
    obj_field_int32_set(enemy,OBJ_F_NPC_EXPERIENCE_WORTH,1000);
    obj_field_int32_set(enemy,OBJ_F_NPC_EXPERIENCE_POOL,1000);
    combat_tactical_enable();combat_critter_activate_combat_mode(pc);
    assert(combat_turn_based_is_active());
    CombatContext damage;sub_4B2210(junior,enemy,&damage);
    damage.flags|=CF_HIT;damage.dam_flags|=CDF_IGNORE_RESISTANCE;damage.dam[DAMAGE_TYPE_NORMAL]=5;
    combat_dmg(&damage);
    assert(!critter_is_dead(enemy));
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==0 && stat_base_get(junior,STAT_EXPERIENCE_POINTS)==0);
    sub_4B2210(pc,enemy,&damage);damage.flags|=CF_HIT;damage.dam_flags|=CDF_DEATH|CDF_IGNORE_RESISTANCE;
    combat_dmg(&damage);assert(critter_is_dead(enemy));
    assert(obj_field_int32_get(enemy,OBJ_F_NPC_EXPERIENCE_WORTH)==0);
    int native_reward=modern_compat_get()->no_xp_per_hit?1000:
        1200-obj_field_int32_get(enemy,OBJ_F_NPC_EXPERIENCE_POOL);
    modern_combat_xp_add(junior,2200-native_reward);
    progression_settle();assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==0);
    modern_encounter_started();
    ObjectID junior_id=obj_get_id(junior),veteran_id=obj_get_id(veteran);
    assert(gamelib_save("SlotP001","Progression - deferred combat XP"));
    assert(gamelib_load("SlotP001"));
    pc=player_get_local_pc_obj();junior=obj_pool_perm_lookup(junior_id);veteran=obj_pool_perm_lookup(veteran_id);
    assert(junior && veteran && combat_turn_based_is_active());
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==0 && modern_encounter_grace_remaining()==5000);
    combat_turn_based_whos_turn_set(pc);sub_423FF0(pc);combat_critter_deactivate_combat_mode(pc);
    assert(!combat_turn_based_is_active());progression_settle();
    printf("Combat XP: hero=%d junior=%d veteran=%d\n",stat_base_get(pc,STAT_EXPERIENCE_POINTS),
        stat_base_get(junior,STAT_EXPERIENCE_POINTS),stat_base_get(veteran,STAT_EXPERIENCE_POINTS));
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2200);
    assert(stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2200);
    assert(stat_base_get(veteran,STAT_EXPERIENCE_POINTS)==level_experience_for_level(5)+2200);
    assert(stat_base_get(pc,STAT_LEVEL)==2 && stat_base_get(junior,STAT_LEVEL)==2 && stat_base_get(veteran,STAT_LEVEL)==5);
    assert(stat_base_get(junior,STAT_UNSPENT_POINTS)==junior_points+1);
    progression_settle();assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2200);
    assert(gamelib_save("SlotP002","Progression - combat XP paid"));
    assert(gamelib_load("SlotP002"));pc=player_get_local_pc_obj();junior=obj_pool_perm_lookup(junior_id);
    progression_settle();assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2200);
    assert(stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2200);
    puts("PASS: native hits/death deferred, equal party XP, independent follower leveling, saved combat paid exactly once");

    /* Without a turn-based fight, settle only after the party is safe. */
    critter_fatigue_damage_set(junior,critter_fatigue_max(junior)+10);
    assert(critter_is_unconscious(junior));
    modern_combat_xp_add(pc,100);progression_settle();
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2300 && stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2300);
    critter_fatigue_damage_set(junior,0);
    critter_give_xp(pc,50);assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2350);
    puts("PASS: unconscious living companions share XP; quest XP outside combat remains immediate");

    assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(33,30),&enemy));
    obj_field_int32_set(enemy,OBJ_F_NPC_FLAGS,obj_field_int32_get(enemy,OBJ_F_NPC_FLAGS)|ONF_FIGHTING);
    obj_field_handle_set(enemy,OBJ_F_NPC_COMBAT_FOCUS,junior);
    assert(!combat_critter_is_combat_mode_active(pc));
    modern_combat_xp_add(junior,100);progression_settle();
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2350);
    obj_field_int32_set(enemy,OBJ_F_NPC_FLAGS,obj_field_int32_get(enemy,OBJ_F_NPC_FLAGS)&~ONF_FIGHTING);
    progression_settle();assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2450);
    assert(stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2400);
    settings_set_value(&settings,DIFFICULTY_KEY,GAME_DIFFICULTY_EASY);
    modern_combat_xp_add(pc,100);progression_settle();
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2600 && stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2550);
    settings_set_value(&settings,DIFFICULTY_KEY,GAME_DIFFICULTY_HARD);
    modern_combat_xp_add(pc,100);progression_settle();
    assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2650 && stat_base_get(junior,STAT_EXPERIENCE_POINTS)==2600);
    settings_set_value(&settings,DIFFICULTY_KEY,GAME_DIFFICULTY_NORMAL);
    critter_give_xp(pc,-50);assert(stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2600);
    puts("PASS: real-time combat with passive hero, difficulty applied once, scripted XP deductions preserved");

    assert(level_experience_for_level(50)==1300000 && level_maximum()>100);
    for(int level=2;level<=level_maximum();level++)
        assert(level_experience_for_level(level)>level_experience_for_level(level-1));
    level_set(pc,50);critter_give_xp(pc,level_experience_points_to_next_level(pc));
    assert(stat_base_get(pc,STAT_LEVEL)==51);
    level_set(pc,100);critter_give_xp(pc,level_experience_points_to_next_level(pc));
    assert(stat_base_get(pc,STAT_LEVEL)==101 && level_experience_points_to_next_level(pc)>0);
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));capture_frame("level-101");charedit_close();
    level_set(pc,level_maximum());critter_give_xp(pc,2000000000);
    assert(stat_base_get(pc,STAT_LEVEL)==level_maximum() && stat_base_get(pc,STAT_EXPERIENCE_POINTS)==2000000000);
    assert(level_progress_towards_next_level(pc)==0);
    printf("PASS: level 51/101, increasing XP thresholds and safe integer ceiling (level %d)\n",level_maximum());

    assert(modern_progression_save());
    TigFile* invalid=tig_file_fopen("Save\\Current\\modern-progression.dat","wb");assert(invalid);
    int version=99;assert(tig_file_fwrite(&version,sizeof(version),1,invalid)==1);tig_file_fclose(invalid);
    assert(!modern_progression_load());
    tig_file_remove("Save\\Current\\modern-progression.dat");
    assert(modern_progression_load() && modern_encounter_grace_remaining()==0);
    puts("PASS: corrupt extra state rejected; legacy saves without extra state accepted");
}
