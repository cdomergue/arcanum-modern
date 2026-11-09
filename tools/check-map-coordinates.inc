static void map_check_text(const char* text)
{
    SDL_Event event={0};event.type=SDL_EVENT_TEXT_INPUT;event.text.text=text;
    assert(handle_event(&event));modern_ui_tick();
}
static void map_check_key(SDL_Scancode key,SDL_Keymod mod)
{
    SDL_Event event={0};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=key;event.key.mod=mod;
    assert(handle_event(&event));modern_ui_tick();
    event.type=SDL_EVENT_KEY_UP;assert(handle_event(&event));modern_ui_tick();
}
static void map_check_hover(int x,int y)
{
    map_camera.x=x-map_camera.width/2;map_camera.y=y-map_camera.height/2;
    map_camera_clamp(&map_camera,map_camera_width,map_camera_height);capture_frame(NULL);
    int width,height;SDL_GetWindowSize(window,&width,&height);
    float px=map_destination.x+(x-map_camera.x)*map_camera.scale;
    float py=map_destination.y+(y-map_camera.y)*map_camera.scale;
    SDL_WarpMouseInWindow(window,px*width/canvas_w,py*height/canvas_h);capture_frame(NULL);
}
static void check_map_coordinates(void)
{
    wmap_ui_open();panels_tick();modern_map_action(1);capture_frame(NULL);assert(map_camera_local);
    int west,south,x,y,check_w,check_s;
    assert(modern_map_position_coords(&west,&south));
    assert(modern_companions_coordinates(map_current_map(),obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION),&check_w,&check_s));
    assert(west==check_w && south==check_s);
    assert(modern_map_coords_point(west,south,&x,&y));
    assert(modern_map_point_coords(x,y,&check_w,&check_s) && check_w==west && check_s==south);
    assert(!modern_map_point_coords(-1,y,&check_w,&check_s));
    assert(!modern_map_point_coords(map_local.width,y,&check_w,&check_s));
    assert(!modern_map_coords_point(INT_MAX,INT_MAX,&x,&y));
    assert(!modern_map_coords_point(-1,0,&x,&y));
    assert(modern_map_coords_point(west,south,&x,&y));
    int route_count=map_local.count,original_count=map_memo_count;
    assert(original_count<63);
    for(int scale=100;scale<=150;scale+=50) {
        text_scale=scale;capture_frame(NULL);map_check_hover(x,y);
        char name[80];snprintf(name,sizeof(name),"map-coordinates-local-%d",scale);capture_frame(name);
        float old_scale=map_camera.scale;
        map_zoom(1.3f,map_destination.x+map_destination.w/2,map_destination.y+map_destination.h/2);
        capture_frame(NULL);map_check_hover(x,y);
        snprintf(name,sizeof(name),"map-coordinates-zoom-%d",scale);capture_frame(name);
        assert(map_camera.scale>=old_scale);
        assert(modern_map_point_coords(x,y,&check_w,&check_s) && check_w==west && check_s==south);
    }
    click_action(2199);capture_frame("map-note-menu-fr-150");
    map_check_key(SDL_SCANCODE_ESCAPE,0);assert(!memo_menu_open && native_panel()==3);
    click_action(2199);
    SDL_Event outside={0};outside.type=SDL_EVENT_MOUSE_BUTTON_DOWN;outside.button.button=SDL_BUTTON_LEFT;outside.button.down=true;
    int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
    outside.button.x=(map_destination.x+10)*ww/canvas_w;outside.button.y=(map_destination.y+map_destination.h-10)*wh/canvas_h;
    assert(handle_event(&outside));modern_ui_tick();assert(memo_menu_open);
    outside.type=SDL_EVENT_MOUSE_BUTTON_UP;outside.button.down=false;
    assert(handle_event(&outside));modern_ui_tick();assert(!memo_menu_open && !map_pressed && map_local.count==route_count && memo_edit<0);
    click_action(2199);assert(memo_menu_open);click_action(2195);assert(memo_edit>=0 && memo_field==1);
    map_check_text("999999999999");map_check_key(SDL_SCANCODE_TAB,0);assert(memo_field==2);
    char value[32];snprintf(value,sizeof(value),"%d",south);map_check_text(value);
    map_check_key(SDL_SCANCODE_TAB,0);assert(memo_field==0);
    map_check_text("Repère précis — rendez-vous près de la bibliothèque.");
    map_check_key(SDL_SCANCODE_RETURN,0);assert(memo_edit>=0 && memo_error==1 && map_memo_count==original_count);
    capture_frame("map-coordinates-invalid");
    click_action(2197);snprintf(value,sizeof(value),"%d",west);map_check_text(value);
    capture_frame("map-coordinates-editor-fr-150");
    float zoom_before=map_camera.scale;SDL_Event wheel={0};wheel.type=SDL_EVENT_MOUSE_WHEEL;wheel.wheel.y=1;
    assert(handle_event(&wheel));assert(map_camera.scale==zoom_before);
    map_check_key(SDL_SCANCODE_RETURN,0);assert(memo_edit<0 && map_memo_count==original_count+1);
    MapMemo saved=map_memos[original_count];assert(saved.precise && saved.west==west && saved.south==south);
    assert(saved.x==x && saved.y==y && saved.townmap==memo_map_state.townmap);
    notebook_identity[0]=0;notebook_sync();assert(map_memo_count==original_count+1);
    assert(map_memos[original_count].precise && map_memos[original_count].west==west && map_memos[original_count].south==south);
    capture_frame(NULL);map_check_hover(x,y);capture_frame("map-coordinates-note");
    assert(memo_action(2200+original_count));assert(memo_edit==original_count);
    /* Text-only edits preserve the clicked/native pixel position. */
    map_check_key(SDL_SCANCODE_A,SDL_KMOD_CTRL);map_check_text("Repère conservé");
    click_action(2192);assert(map_memos[original_count].x==x && map_memos[original_count].y==y);
    assert(map_local.count==route_count);
    /* Existing placement by a map click is still available. */
    click_action(2199);click_action(2190);assert(memo_placing && !memo_menu_open);
    click_action(2199);assert(!memo_placing && memo_menu_open);click_action(2199);assert(!memo_menu_open);
    modern_map_action(2);capture_frame(NULL);assert(memo_map_state.mode==1);
    int cx,cy;assert(modern_map_coords_point(west,south,&cx,&cy));
    assert(modern_map_point_coords(cx,cy,&check_w,&check_s));
    assert(abs(check_w-west)<10 && abs(check_s-south)<10);
    map_check_hover(cx,cy);capture_frame("map-coordinates-continent");
    click_action(2199);assert(memo_menu_open);click_action(2195);assert(memo_field==1);
    /* The overview marker is coarser than the entered coordinates; the
     * precise values must nevertheless survive closing and reopening. */
    snprintf(value,sizeof(value),"%d",west);map_check_text(value);
    map_check_key(SDL_SCANCODE_TAB,0);snprintf(value,sizeof(value),"%d",south);map_check_text(value);
    map_check_key(SDL_SCANCODE_TAB,0);map_check_text("Repère du continent");click_action(2192);
    assert(map_memo_count==original_count+2);capture_frame(NULL);
    assert(memo_action(2200+original_count+1));assert(atoi(memo_west)==west && atoi(memo_south)==south);
    char* language=(char*)modern_intgame_text(0);char original_language[64];
    assert(strlen(language)>=5 && strlen(language)<sizeof(original_language));strcpy(original_language,language);strcpy(language,"Level");
    capture_frame("map-coordinates-editor-en-150");memo_close();capture_frame("map-coordinates-en-150");
    click_action(2199);capture_frame("map-note-menu-en-150");map_check_key(SDL_SCANCODE_ESCAPE,0);
    strcpy(language,original_language);
    modern_map_action(1);capture_frame(NULL);
    if(map_camera_local)modern_map_action(2);
    /* World view and overview use the same compass, without invoking the
     * native mouse handlers that can append route points. */
    modern_map_action(1);capture_frame(NULL);
    if(map_camera_travel) {
        assert(modern_map_coords_point(west,south,&cx,&cy) && cx==west && cy==south);
        assert(modern_map_point_coords(cx,cy,&check_w,&check_s) && check_w==west && check_s==south);
        map_check_hover(cx,cy);capture_frame("map-coordinates-travel");
    }
    SDL_SetWindowSize(window,800,600);text_scale=150;capture_frame("map-coordinates-800x600-150");
    click_action(2199);assert(memo_menu_open);capture_frame("map-note-menu-800x600-150");click_action(2195);capture_frame("map-coordinates-editor-800x600-150");memo_close();
    /* Render the actual revised lesson in the isolated fixture. */
    int64_t guide;assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION),&guide));
    ObjectID guide_id;assert(objid_id_from_str(&guide_id,"G_9446D492_810B_D640_A404_25CBFAC90F01"));obj_pool_perm_oid_set(guide_id,guide);
    Script guide_script={0};guide_script.num=48874;obj_arrayfield_script_set(guide,OBJ_F_SCRIPTS_IDX,SAP_DIALOG,&guide_script);
    tutorial_available=true;tutorial_starting=tutorial_new_session=false;
    script_local_counter_set(guide,SAP_DIALOG,0,TUTORIAL_NOTE);capture_frame(NULL);
    assert(tutorial_voice_id && !strcmp(tutorial_voice_id,"TUTORIAL_NOTE"));
    assert(tutorial_voice!=TIG_SOUND_HANDLE_INVALID && tig_sound_is_active(tutorial_voice));
    click_action(2199);capture_frame("map-note-tutorial-fr-800x600-150");
    click_action(2195);capture_frame("map-note-tutorial-editor-fr-800x600-150");
    map_check_key(SDL_SCANCODE_TAB,0);map_check_key(SDL_SCANCODE_TAB,0);map_check_text("Repère du tutoriel");click_action(2192);capture_frame(NULL);
    assert((script_local_counter_get(guide,SAP_DIALOG,0)&255)==TUTORIAL_JOURNAL);
    tutorial_available=false;tutorial_voice_stop();
    /* Keep repeated runs independent and leave original test-copy notes intact. */
    map_memo_count=original_count;assert(notebook_save());wmap_ui_close();
    puts("PASS: map position/cursor compass, journal agreement, zoom, validation, numeric entry, modal keyboard, NOTE3 persistence and FR/EN rendering");
}
