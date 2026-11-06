static void check_maintained_scroll(int64_t pc)
{
    int64_t scroll;
    assert(object_create(sub_4685A0(11079),obj_field_int64_get(pc,OBJ_F_LOCATION),&scroll));
    assert(item_transfer(scroll,pc));
    assert(mt_item_spell(scroll,0)==SPELL_AGILITY_OF_FIRE);
    obj_field_int32_set(scroll,OBJ_F_ITEM_FLAGS,obj_field_int32_get(scroll,OBJ_F_ITEM_FLAGS)|OIF_IDENTIFIED);
    int dexterity=stat_level_get(pc,STAT_DEXTERITY);
    assert(modern_hotkey_assign(1,HOTKEY_ITEM_SPELL,SPELL_AGILITY_OF_FIRE,scroll));
    modern_hotkey_activate(1);
    TargetDescriptor target;target_descriptor_set_obj(&target,pc);item_ui_apply(&target);
    int ids[32],spells[32],count=0;
    for(int frame=0;frame<300;frame++) {
        tig_ping();gamelib_ping();modern_ui_tick();SDL_Delay(20);
        count=modern_maintained_spells(pc,ids,spells,32);
        if(count && stat_level_get(pc,STAT_DEXTERITY)>dexterity && combat_input_ready(pc))break;
    }
    assert(stat_level_get(pc,STAT_DEXTERITY)>dexterity);
    assert(count==1 && spells[0]==SPELL_AGILITY_OF_FIRE);
    MagicTechRunInfo* run;assert(magictech_id_to_run_info(ids[0],&run));
    assert(run->parent_obj.obj==pc && run->source_obj.obj!=pc);
    assert(!obj_handle_is_valid(scroll) || (obj_field_int32_get(scroll,OBJ_F_FLAGS)&(OF_DESTROYED|OF_EXTINCT)));
    capture_frame("maintained-scroll");assert(maintained_count==1);
    int64_t followers[1];assert(modern_followers_get(followers,1)==1);
    assert(modern_maintained_spells(followers[0],ids+1,spells+1,31)==0);
    assert(!modern_spell_stop(followers[0],ids[0]));
    click_action(2500);
    for(int frame=0;frame<100 && stat_level_get(pc,STAT_DEXTERITY)!=dexterity;frame++) {
        tig_ping();gamelib_ping();modern_ui_tick();SDL_Delay(20);
    }
    assert(stat_level_get(pc,STAT_DEXTERITY)==dexterity);
    assert(modern_maintained_spells(pc,ids,spells,32)==0);
    capture_frame("maintained-scroll-stopped");
    puts("PASS: hotbar scroll cast, consumption, maintained icon, caster ownership and interruption");
}
