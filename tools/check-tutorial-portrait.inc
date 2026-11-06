/* Native lesson transitions in the harness's isolated game fixture. */
static void check_tutorial_portrait(int64_t pc)
{
    int64_t guide;
    assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(32,30),&guide));
    ObjectID id;
    assert(objid_id_from_str(&id,"G_9446D492_810B_D640_A404_25CBFAC90F01"));
    obj_pool_perm_oid_set(id,guide);
    Script script={0};script.num=48874;
    obj_arrayfield_script_set(guide,OBJ_F_SCRIPTS_IDX,SAP_DIALOG,&script);
    tutorial_available=true;tutorial_starting=tutorial_new_session=false;
    int original=portrait_get(pc);
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));
    script_local_counter_set(guide,SAP_DIALOG,0,TUTORIAL_SHEET);
    capture_frame(NULL);
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_PORTRAIT);
    const int scales[]={100,150};
    for(int i=0;i<2;i++) {
        text_scale=scales[i];char name[64];
        snprintf(name,sizeof(name),"tutorial-portrait-sheet-%d",scales[i]);capture_frame(name);
        assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_PORTRAIT);
    }
    assert(modern_tutorial_active());
    assert(tutorial_voice_id && !strcmp(tutorial_voice_id,"TUTORIAL_PORTRAIT"));
    assert(tutorial_voice!=TIG_SOUND_HANDLE_INVALID);
    click_action(3000);
    assert(portrait_editor.active && portrait_editor.choosing);
    capture_frame("tutorial-portrait-cancel-150");
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_PORTRAIT);
    click_action(514);capture_frame(NULL);
    assert(!portrait_editor.active && portrait_get(pc)==original);
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_SPELLS);
    capture_frame(NULL);
    script_local_counter_set(guide,SAP_DIALOG,0,TUTORIAL_PORTRAIT);
    capture_frame(NULL);
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_PORTRAIT);
    tutorial_skip=true;capture_frame(NULL);
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_SPELLS);
    assert(portrait_get(pc)==original);
    tutorial_voice_stop();
    puts("PASS: sheet -> portrait -> spells, cancel preserves portrait, optional skip and active tutorial");
}
