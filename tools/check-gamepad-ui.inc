/* Included by check-button-text.c; requires its isolated game marker. */
#include "game/tile.h"
#include "game/tile_block.h"
#include "game/a_name.h"
#include "game/sector.h"
static SDL_Joystick* check_pad_joystick;
static int check_pad_pulses;
static bool SDLCALL check_pad_rumble(void* context,Uint16 low,Uint16 high)
{
    (void)context;if(low || high)check_pad_pulses++;return true;
}
static void check_pad_frame(const char* name)
{
    SDL_UpdateJoysticks();SDL_Event e;
    while(SDL_PollEvent(&e))handle_event(&e);
    capture_frame(name);
}
static void check_pad_press(SDL_GamepadButton button)
{
    assert(SDL_SetJoystickVirtualButton(check_pad_joystick,button,true));check_pad_frame(NULL);
    SDL_Delay(120);check_pad_frame(NULL);
}
static void check_pad_release(SDL_GamepadButton button)
{
    assert(SDL_SetJoystickVirtualButton(check_pad_joystick,button,false));check_pad_frame(NULL);
}
static void check_pad_tap(SDL_GamepadButton button)
{
    /* Submit the release after the delayed hold, before another render. A slow
     * software capture must not turn a short tap into an autorepeated hold.
     * check_pad_press separately exercises buttons held across sampled frames. */
    assert(SDL_SetJoystickVirtualButton(check_pad_joystick,button,true));check_pad_frame(NULL);
    SDL_Delay(120);check_pad_release(button);
}
static void check_pad_focus_action(int action)
{
    check_pad_frame(NULL);pad_cursor_mode=false;
    for(int i=0;i<pad_focus_count;i++)if(pad_focus_nodes[i].action==action){pad_focus_index=i;return;}
    fprintf(stderr,"Missing pad focus action %d on screen %d\n",action,pad_screen());assert(false);
}
static void check_gamepad_ui(void)
{
    /* ShopMap's blank floor is impassable. Create a real, walkable test patch
     * in memory, confined to this disposable fixture, before testing movement. */
    mes_file_handle_t tiles;assert(mes_load("art\\tile\\tilename.mes",&tiles));
    int count=mes_entries_count_in_range(tiles,0,99);tig_art_id_t floor=TIG_ART_ID_INVALID;
    for(int i=0;i<count;i++) {
        tig_art_id_t candidate;tig_art_tile_id_create(i,i,15,0,1,1,1,0,&candidate);
        if(!a_name_tile_is_blocking(candidate) && tig_art_exists(candidate)==TIG_OK){floor=candidate;break;}
    }
    mes_unload(tiles);assert(floor!=TIG_ART_ID_INVALID);
    for(int y=20;y<=42;y++)for(int x=20;x<=42;x++) {
        int64_t tile=location_make(x,y),id=sector_id_from_loc(tile);Sector* sector;
        assert(sector_lock(id,&sector));sector->tiles.art_ids[tile_id_from_loc(tile)]=floor;sector_unlock(id);
        tileblock_set_tile_blocked(tile,false);assert(!tile_is_blocking(tile,false));
    }
    assert(SDL_SetWindowSize(window,1280,800));SDL_HideWindow(window);SDL_ShowWindow(window);SDL_RaiseWindow(window);
    fprintf(stderr,"Controller test waiting for focus: driver=%s window=%u\n",SDL_GetCurrentVideoDriver(),SDL_GetWindowID(window));
    for(int i=0;i<3000 && check_keyboard_focus()!=window;i++){SDL_PumpEvents();SDL_Delay(20);}
    assert(check_keyboard_focus()==window);
    SDL_VirtualJoystickDesc desc;SDL_INIT_INTERFACE(&desc);
    desc.vendor_id=0x1209;desc.product_id=0xa11c;
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD;desc.naxes=SDL_GAMEPAD_AXIS_COUNT;desc.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
    desc.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;desc.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
    desc.name="Renaissance UI gamepad";desc.Rumble=check_pad_rumble;
    SDL_JoystickID id=SDL_AttachVirtualJoystick(&desc);assert(id);check_pad_frame(NULL);
    assert(gamepad.device && gamepad.id==id);check_pad_joystick=SDL_GetGamepadJoystick(gamepad.device);
    check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);
    assert(native_panel()==0 && !mainmenu_ui_is_active() && !modern_ui_take_pause_request());
    check_pad_press(SDL_GAMEPAD_BUTTON_START);assert(modern_ui_take_pause_request());
    modern_mainmenu_pause_start();check_pad_frame("gamepad-pause-1280x800");
    assert(mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_MAINMENU_IN_PLAY);
    check_pad_frame(NULL);assert(mainmenu_ui_is_active());
    check_pad_release(SDL_GAMEPAD_BUTTON_START);
    int pause_pulses=check_pad_pulses;
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(check_pad_pulses==pause_pulses+1);
    SDL_Delay(60);check_pad_frame(NULL);assert(check_pad_pulses==pause_pulses+1);
    gamepad.rumble=false;check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(check_pad_pulses==pause_pulses+1);gamepad.rumble=true;
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);check_pad_pulses=0;
    assert(!mainmenu_ui_is_active() && native_panel()==0);
    mainmenu_ui_window_type_set(MM_WINDOW_MAINMENU);mainmenu_ui_open();check_pad_frame(NULL);
    int main_pulses=check_pad_pulses;
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(check_pad_pulses==main_pulses+1);
    SDL_Delay(60);check_pad_frame("gamepad-main-menu-1280x800");assert(check_pad_pulses==main_pulses+1);
    gamepad.rumble=false;check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(check_pad_pulses==main_pulses+1);gamepad.rumble=true;
    mainmenu_ui_close(false);check_pad_frame(NULL);
    int64_t dagger;assert(object_create(sub_4685A0(BP_DAGGER),obj_field_int64_get(player_get_local_pc_obj(),OBJ_F_LOCATION),&dagger));
    assert(item_transfer(dagger,player_get_local_pc_obj()));
    check_pad_press(SDL_GAMEPAD_BUTTON_WEST);assert(native_panel()==2);
    check_pad_release(SDL_GAMEPAD_BUTTON_WEST);assert(native_panel()==2);
    check_pad_frame("gamepad-inventory-1280x800");assert(pad_focus_count>20);
    int previous=pad_focus_index;check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(pad_focus_index!=previous);
    /* Navigate the real grid with the analog stick, not injected focus IDs. */
    int grid=-1;
    for(int i=0;i<pad_focus_count;i++)if(pad_focus_nodes[i].action==-2){grid=i;break;}
    assert(grid>=0);pad_focus_index=grid;
    int grid_x=pad_focus_nodes[grid].x,grid_y=pad_focus_nodes[grid].y;
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,24000));check_pad_frame(NULL);
    assert(pad_focus_nodes[pad_focus_index].action==-2 && pad_focus_nodes[pad_focus_index].x==grid_x+32 && pad_focus_nodes[pad_focus_index].y==grid_y);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,0));check_pad_frame(NULL);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,24000));check_pad_frame(NULL);
    assert(pad_focus_nodes[pad_focus_index].action==-2 && pad_focus_nodes[pad_focus_index].x==grid_x+32 && pad_focus_nodes[pad_focus_index].y==grid_y+32);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,0));check_pad_frame(NULL);
    assert(!inven_ui_drag_item_obj_get());check_pad_frame("gamepad-inventory-navigation-1280x800");
    int item_node=-1;
    for(int i=0;i<pad_focus_count;i++)if(pad_focus_nodes[i].action<0 && modern_inventory_at(pad_focus_nodes[i].x,pad_focus_nodes[i].y)==dagger){item_node=i;break;}
    assert(item_node>=0);pad_focus_index=item_node;
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(inven_ui_drag_item_obj_get()==dagger);
    check_pad_frame("gamepad-inventory-pickup-1280x800");
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(!inven_ui_drag_item_obj_get());
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(inven_ui_drag_item_obj_get()==dagger);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(!inven_ui_drag_item_obj_get() && native_panel()==2);
    check_pad_tap(SDL_GAMEPAD_BUTTON_WEST);assert(pad_binding.type==HOTKEY_ITEM && pad_binding.item==dagger);
    check_pad_frame("gamepad-assign-item-1280x800");
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(!pad_binding.type);
    assert(modern_hotkey_get(0)->type==HOTKEY_ITEM && modern_hotkey_get(0)->item_obj.obj==dagger);
    check_pad_press(SDL_GAMEPAD_BUTTON_EAST);assert(native_panel()==0);
    check_pad_release(SDL_GAMEPAD_BUTTON_EAST);assert(!mainmenu_ui_is_active());
    int64_t sheet_pc=player_get_local_pc_obj();
    int points_before=stat_base_get(sheet_pc,STAT_UNSPENT_POINTS);stat_base_set(sheet_pc,STAT_UNSPENT_POINTS,10);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(native_panel()==1);
    check_pad_focus_action(692);check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(pad_focus_nodes[pad_focus_index].action==693);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(pad_focus_nodes[pad_focus_index].action==692);
    int strength_before=stat_base_get(sheet_pc,STAT_STRENGTH);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    assert(stat_base_get(sheet_pc,STAT_STRENGTH)==strength_before+1 && stat_base_get(sheet_pc,STAT_UNSPENT_POINTS)==9);
    assert(pad_focus_nodes[pad_focus_index].action==692);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(stat_base_get(sheet_pc,STAT_STRENGTH)==strength_before);
    for(int i=0;i<9;i++)check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(pad_focus_nodes[pad_focus_index].action==691);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==651);
    check_pad_tap(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(pad_focus_nodes[pad_focus_index].action==696);
    check_pad_tap(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(pad_focus_nodes[pad_focus_index].action==692);
    check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    assert(pad_focus_nodes[pad_focus_index].action==651);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    assert(pad_focus_nodes[pad_focus_index].action==660);
    int skill_before=basic_skill_points_get(sheet_pc,sheet_skills[0]);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(basic_skill_points_get(sheet_pc,sheet_skills[0])>skill_before);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(basic_skill_points_get(sheet_pc,sheet_skills[0])==skill_before);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==661);
    check_pad_frame("gamepad-character-1280x800");
    assert(SDL_SetWindowSize(window,800,600));text_scale=150;check_pad_frame("gamepad-character-800x600-150");
    assert(SDL_SetWindowSize(window,1280,800));text_scale=100;check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(native_panel()==0);
    stat_base_set(sheet_pc,STAT_UNSPENT_POINTS,points_before);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(journal);
    check_pad_frame("gamepad-journal-1280x800");check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(!journal);
    int64_t followers[8];int follower_count=modern_followers_get(followers,8);assert(follower_count>0);
    assert(inven_ui_create(player_get_local_pc_obj(),followers[0],INVEN_UI_MODE_BARTER));check_pad_frame(NULL);
    check_pad_frame("gamepad-trade-1280x800");assert(pad_focus_count>20);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(native_panel()==0);
    dialog_ui_start_dialog(player_get_local_pc_obj(),followers[0],1079,0,1);check_pad_frame(NULL);
    assert(modern_dialog_state() && modern_dialog_state()->num_options>0);
    check_pad_frame("gamepad-dialogue-1280x800");
    for(int i=0;i<pad_focus_count;i++)assert(pad_focus_nodes[i].action>=800 && pad_focus_nodes[i].action<=823);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    dialog_ui_end_dialog(player_get_local_pc_obj(),0);check_pad_frame(NULL);
    for(int i=0;i<30;i++){SDL_Delay(20);tig_ping();gamelib_ping();}
    check_pad_press(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);assert(pad_radial);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,-32767));check_pad_frame(NULL);
    assert(pad_radial_slot==0);check_pad_frame("gamepad-radial-1280x800");
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767));check_pad_frame(NULL);
    assert(pad_radial_offset==10);check_pad_frame("gamepad-radial-alt-1280x800");
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768));check_pad_frame(NULL);
    assert(pad_radial_offset==0);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(!pad_radial && gamepad.neutral && !gamepad.left.length && !gamepad.right.length);
    check_pad_release(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,0));check_pad_frame(NULL);
    assert(modern_hotkey_assign(10,HOTKEY_SKILL,1,OBJ_HANDLE_NULL));
    check_pad_press(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,-32767));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767));check_pad_frame(NULL);
    assert(pad_radial_offset==10 && pad_radial_slot==0);
    check_pad_release(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    assert(intgame_mode_get()==INTGAME_MODE_SKILL && sub_579F90()==1);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,0));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768));check_pad_frame(NULL);
    check_pad_frame(NULL);assert(pad_cursor_mode);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,18000));check_pad_frame(NULL);
    TigMouseState aimed;assert(tig_mouse_get_state(&aimed)==TIG_OK);
    assert(abs(aimed.x-(int)(mouse_x*hrp_iso_window_width_get()/canvas_w))<=1);
    check_pad_frame("gamepad-skill-cursor-1280x800");
    assert(tig_art_num_get(tig_mouse_cursor_get_art_id())==352);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,18000));check_pad_frame(NULL);
    assert(follow_walk.pc && intgame_mode_get()==INTGAME_MODE_SKILL && pad_cursor_mode);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,0));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,0));check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(intgame_mode_get()==INTGAME_MODE_MAIN);
    /* Every menu sector dispatches its actual native screen. */
    const int expected_panel[]={1,0,3,2,5,5,7,6,8};
    for(int entry=0;entry<9;entry++) {
        check_pad_press(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);assert(pad_radial && pad_menu_radial);
        float angle=entry*6.2831853f/9;
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,(Sint16)(sinf(angle)*32767)));
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,(Sint16)(-cosf(angle)*32767)));
        check_pad_frame(entry==0?"gamepad-menu-wheel-1280x800":NULL);assert(pad_radial_slot==entry);
        if(entry==0) {
            assert(SDL_SetWindowSize(window,800,600));text_scale=150;check_pad_frame("gamepad-menu-wheel-800x600-150");
            assert(SDL_SetWindowSize(window,1280,800));text_scale=100;check_pad_frame(NULL);
        }
        check_pad_release(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,0));
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTY,0));check_pad_frame(NULL);
        if(native_panel()!=expected_panel[entry])fprintf(stderr,"Wheel entry %d panel=%d expected=%d\n",entry,native_panel(),expected_panel[entry]);
        assert(native_panel()==expected_panel[entry]);if(entry==1)assert(journal);
        if(entry==2) {
            check_pad_frame(NULL);check_pad_frame(NULL);
            assert(map_camera_active);
            float scale=map_camera.scale;
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767));
            SDL_Delay(80);check_pad_frame(NULL);
            fprintf(stderr,"Map zoom: before=%f after=%f trigger=%d neutral=%d dimensions=%f,%f view=%f,%f\n",scale,map_camera.scale,gamepad.right_trigger,gamepad.neutral,map_camera_width,map_camera_height,map_viewport.w,map_viewport.h);
            assert(map_camera.scale>scale);
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,-32768));check_pad_frame(NULL);
            float map_x=map_camera.x,map_y=map_camera.y;
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,24000));
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,24000));
            SDL_Delay(80);check_pad_frame(NULL);
            assert(map_camera.x!=map_x || map_camera.y!=map_y);
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,0));
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,0));
            check_pad_frame("gamepad-map-pan-1280x800");
        }
        if(entry==4)assert(selector==2);if(entry==5)assert(selector==1);
        if(entry==4) {
            check_pad_focus_action(881);check_pad_tap(SDL_GAMEPAD_BUTTON_WEST);
            assert(pad_binding.type==HOTKEY_SKILL && pad_binding.data==1);
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767));check_pad_frame(NULL);
            check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(modern_hotkey_get(10)->type==HOTKEY_SKILL);
            assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,-32768));check_pad_frame(NULL);
        }
        if(entry==5) {
            spell_college_level_set(player_get_local_pc_obj(),0,1);check_pad_frame(NULL);
            check_pad_focus_action(1400);check_pad_tap(SDL_GAMEPAD_BUTTON_WEST);
            assert(pad_binding.type==HOTKEY_SPELL && pad_binding.data==0);
            check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);
            assert(modern_hotkey_get(1)->type==HOTKEY_SPELL && modern_hotkey_get(1)->data==0);
            check_pad_focus_action(1400);check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);check_pad_frame(NULL);
            assert(intgame_mode_get()==INTGAME_MODE_SPELL && pad_cursor_mode);
            assert(tig_art_num_get(tig_mouse_cursor_get_art_id())==21);
            check_pad_frame("gamepad-spell-cursor-1280x800");
        }
        check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);check_pad_frame(NULL);
        assert(!journal && !native_panel() && !mainmenu_ui_is_active());
    }
    int64_t pc=player_get_local_pc_obj(),front,chest;
    object_set_current_aid(pc,tig_art_id_rotation_set(obj_field_int32_get(pc,OBJ_F_CURRENT_AID),0));
    assert(location_in_dir(obj_field_int64_get(pc,OBJ_F_LOCATION),0,&front));
    assert(object_create(sub_4685A0(BP_WOOD_CHEST_1),front,&chest));
    obj_field_int32_set(chest,OBJ_F_CONTAINER_FLAGS,0);
    pad_cursor_mode=false;pad_target=0;check_pad_frame(NULL);assert(pad_front_target(pc)==chest);
    object_set_current_aid(pc,tig_art_id_rotation_set(obj_field_int32_get(pc,OBJ_F_CURRENT_AID),4));
    assert(pad_front_target(pc)!=chest);
    object_set_current_aid(pc,tig_art_id_rotation_set(obj_field_int32_get(pc,OBJ_F_CURRENT_AID),0));
    assert(anim_goal_walk_to_tile(pc,location_make(33,33)));assert(!combat_input_ready(pc));
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);
    for(int i=0;i<150 && !inven_ui_is_created();i++){SDL_Delay(20);tig_ping();gamelib_ping();check_pad_frame(NULL);}
    assert(inven_ui_is_created());check_pad_frame("gamepad-front-chest-1280x800");
    check_pad_tap(SDL_GAMEPAD_BUTTON_EAST);assert(!native_panel());object_flags_set(chest,OF_OFF);
    int64_t entrance;assert(object_create(sub_4685A0(BP_SMALL_STONE),front,&entrance));
    assert(pad_front_target(pc)!=entrance);
    Script use={0};use.num=1;obj_arrayfield_script_set(entrance,OBJ_F_SCRIPTS_IDX,SAP_USE,&use);
    object_flags_unset(entrance,OF_CLICK_THROUGH);
    object_set_current_aid(pc,tig_art_id_rotation_set(obj_field_int32_get(pc,OBJ_F_CURRENT_AID),location_rot(obj_field_int64_get(pc,OBJ_F_LOCATION),front)));
    if(pad_front_target(pc)!=entrance) {
        TigRect r;object_get_rect(entrance,0,&r);
        fprintf(stderr,"Scenery: chosen=%lld entrance=%lld flags=%x tile=%lld actor=%lld rect=%d,%d,%d,%d\n",(long long)pad_front_target(pc),(long long)entrance,obj_field_int32_get(entrance,OBJ_F_FLAGS),(long long)front,(long long)obj_field_int64_get(pc,OBJ_F_LOCATION),r.x,r.y,r.width,r.height);
    }
    assert(pad_front_target(pc)==entrance);object_flags_set(entrance,OF_OFF);
    int npc_rotation=location_rot(obj_field_int64_get(pc,OBJ_F_LOCATION),obj_field_int64_get(followers[0],OBJ_F_LOCATION));
    object_set_current_aid(pc,tig_art_id_rotation_set(obj_field_int32_get(pc,OBJ_F_CURRENT_AID),npc_rotation));
    assert(pad_front_target(pc)==followers[0]);
    modern_health_reset();check_pad_pulses=0;
    modern_health_change(pc,0,3);assert(!check_pad_pulses);
    check_pad_frame("gamepad-damage-1280x800");assert(check_pad_pulses==1);
    check_pad_frame(NULL);assert(check_pad_pulses==1);
    modern_health_change(pc,3,0);check_pad_frame(NULL);assert(check_pad_pulses==1);
    gamepad.rumble=false;modern_health_change(pc,0,5);check_pad_frame(NULL);assert(check_pad_pulses==1);gamepad.rumble=true;

    /* Analog magnitude chooses walking/running without changing Always Run. */
    int64_t walk_from=obj_field_int64_get(pc,OBJ_F_LOCATION);
    bool always=get_always_run(pc);set_always_run(true);
    pad_cursor_mode=true;
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,18000));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,6000));check_pad_frame(NULL);
    fprintf(stderr,"Analog: ready=%d active=%d focus=%d neutral=%d mode=%d text=%d stick=%.3f,%.3f actor=%lld pc=%lld walking=%lld\n",
        pad_world_ready(),gamepad.active,gamepad.focused,gamepad.neutral,intgame_mode_get(),SDL_TextInputActive(window),gamepad.left.x,gamepad.left.y,
        (long long)follow_control_actor(),(long long)pc,(long long)follow_walk.pc);
    if(!follow_walk.pc) {
        int64_t tile=obj_field_int64_get(pc,OBJ_F_LOCATION),sx,sy;location_xy(tile,&sx,&sy);
        for(int distance=200;distance>=40;distance-=40) {
            int64_t dest=0;bool found=location_at(sx+40+distance,sy+20+distance/3,&dest);AnimPath path;
            bool path_ok=found && anim_preview_path(pc,dest,false,&path);
            fprintf(stderr,"Path probe: distance=%d found=%d path=%d length=%d direct=%lld active=%d tactical=%d tile_block=%d art=%u flags=%u offsets=%d,%d\n",
                distance,found,path_ok,path_ok?path.max:-1,(long long)location_dist(tile,dest),critter_is_active(pc),tactical_active(),
                tile_is_blocking(dest,false),tile_art_id_at(dest),obj_field_int32_get(pc,OBJ_F_SPELL_FLAGS),
                obj_field_int32_get(pc,OBJ_F_OFFSET_X),obj_field_int32_get(pc,OBJ_F_OFFSET_Y));
        }
    }
    assert(follow_walk.pc==pc && !pad_cursor_mode);AnimRunInfo* run=follow_walk_animation();assert(run && run->goals[0].type==AG_MOVE_TO_TILE);
    assert(get_always_run(pc));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,32767));check_pad_frame(NULL);
    run=follow_walk_animation();assert(run && run->goals[0].type==AG_RUN_TO_TILE);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTX,0));
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_LEFTY,0));check_pad_frame(NULL);assert(!follow_walk.pc);
    set_always_run(always);
    /* Let the final walking step finish before asking combat to accept input. */
    for(int i=0;i<40;i++){SDL_Delay(20);tig_ping();gamelib_ping();}
    assert(obj_field_int64_get(pc,OBJ_F_LOCATION)!=walk_from);

    /* Frame-by-frame world zoom with the actual follow camera. The actor must
     * remain centred during the resize, not merely settle after release. */
    location_origin_set(obj_field_int64_get(pc,OBJ_F_LOCATION));
    for(int i=0;i<30;i++){SDL_Delay(16);modern_follow_camera_tick();}
    int64_t worst_zoom_error=0;
    for(int direction=0;direction<2;direction++) {
        SDL_GamepadAxis trigger=direction?SDL_GAMEPAD_AXIS_RIGHT_TRIGGER:SDL_GAMEPAD_AXIS_LEFT_TRIGGER;
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,trigger,32767));
        for(int i=0;i<30;i++) {
            SDL_Delay(16);check_pad_frame(NULL);modern_follow_camera_tick();
            int64_t dx,dy;location_calc_dist_from_screen_center(obj_field_int64_get(pc,OBJ_F_LOCATION),&dx,&dy);
            dx-=obj_field_int32_get(pc,OBJ_F_OFFSET_X);dy-=obj_field_int32_get(pc,OBJ_F_OFFSET_Y);
            if(llabs(dx)>worst_zoom_error)worst_zoom_error=llabs(dx);
            if(llabs(dy)>worst_zoom_error)worst_zoom_error=llabs(dy);
        }
        assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,trigger,-32768));check_pad_frame(NULL);
    }
    fprintf(stderr,"World zoom maximum centre error: %lld raster pixels\n",(long long)worst_zoom_error);
    assert(worst_zoom_error<=2);
    world_zoom=1;world_zoom_sync();modern_follow_camera_tick();
    check_pad_frame("gamepad-transparent-help-1280x800");

    int64_t enemy;assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(32,32),&enemy));
    combat_tactical_enable();combat_critter_activate_combat_mode(pc);combat_critter_activate_combat_mode(enemy);
    combat_turn_based_whos_turn_set(pc);check_pad_frame(NULL);check_pad_frame(NULL);
    for(int i=0;i<128 && pad_target!=enemy;i++)check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    if(pad_target!=enemy)fprintf(stderr,"Target: world=%d ready=%d blocked=%d target=%lld enemy=%lld actor=%lld\n",
        pad_world_ready(),combat_input_ready(pc),modern_combat_input_blocked(),(long long)pad_target,(long long)enemy,(long long)combat_input_actor());
    assert(pad_target==enemy);
    int64_t second_enemy;assert(object_create(sub_4685A0(BP_BASIC_MALE_NPC),location_make(33,32),&second_enemy));
    combat_critter_activate_combat_mode(second_enemy);check_pad_frame(NULL);
    /* A stick click must take ownership from a pointer parked over the HUD,
     * including a simultaneous stick deflection held through release. */
    pad_cursor_mode=true;mouse_x=canvas_w/2;mouse_y=canvas_h-10;
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,24000));
    check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    assert(pad_target==second_enemy && !pad_cursor_mode);
    SDL_Delay(120);check_pad_frame(NULL);assert(pad_target==second_enemy && !pad_cursor_mode);
    assert(SDL_SetJoystickVirtualAxis(check_pad_joystick,SDL_GAMEPAD_AXIS_RIGHTX,0));check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_STICK);assert(pad_target==enemy && !pad_cursor_mode);
    ModernCombatPreview preview;assert(pad_combat_preview(&preview) && preview.target==enemy);
    check_pad_frame("gamepad-combat-target-1280x800");
    assert(SDL_SetWindowSize(window,800,600));text_scale=150;check_pad_frame("gamepad-combat-help-800x600-150");
    assert(SDL_SetWindowSize(window,1280,800));text_scale=100;check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);
    AnimID attack;assert(anim_is_attacking(pc,&attack,enemy));
    /* The following exit test needs no pending attack. The native guard
     * correctly refuses to leave while this deliberately queued attack runs. */
    assert(anim_interrupt(&attack,PRIORITY_HIGHEST));assert(combat_can_exit_combat_mode(pc));
    combat_turn_based_whos_turn_set(enemy);check_pad_frame(NULL);
    check_pad_tap(SDL_GAMEPAD_BUTTON_NORTH);assert(!combat_critter_is_combat_mode_active(pc) && !combat_turn_based_is_active());
    check_pad_frame(NULL);

    /* Real options screen, with all tabs, pagination and footer in the capture. */
    pad_cursor_mode=true;
    mainmenu_ui_window_type_set(MM_WINDOW_OPTIONS);mainmenu_ui_open();check_pad_frame(NULL);
    assert(mainmenu_ui_is_active() && !pad_cursor_mode);
    check_pad_focus_action(407);check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==411);
    int display_mode=modern_graphics.selected.mode;
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(modern_graphics.selected.mode==(display_mode+2)%3);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(modern_graphics.selected.mode==display_mode);
    /* Reach camera mode exclusively through vertical input, including when
     * borderless mode disables the entire resolution row. */
    while(modern_graphics.selected.mode!=1)check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==446);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==429);
    bool camera_before=follow_camera;
    check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);assert(follow_camera!=camera_before);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(follow_camera==camera_before);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==427);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_UP);assert(pad_focus_nodes[pad_focus_index].action==429);
    check_pad_frame("gamepad-graphics-1280x800");
    const int option_tabs[]={406,408,409};
    const char* option_captures[]={"gamepad-options-game-1280x800","gamepad-options-video-1280x800","gamepad-options-sound-1280x800"};
    for(int tab=0;tab<3;tab++) {
        check_pad_focus_action(option_tabs[tab]);check_pad_tap(SDL_GAMEPAD_BUTTON_SOUTH);
        check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);
        int action=pad_focus_nodes[pad_focus_index].action;
        assert(action>=431 && action<446 && action%2);
        int index=(action-430)/2;ModernOption before,after;
        assert(modern_options_state(index,&before));
        SDL_GamepadButton direction=before.numeric && before.value==before.maximum?SDL_GAMEPAD_BUTTON_DPAD_LEFT:SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
        check_pad_tap(direction);assert(modern_options_state(index,&after) && after.value!=before.value);
        assert(pad_focus_nodes[pad_focus_index].action==action);
        check_pad_tap(direction==SDL_GAMEPAD_BUTTON_DPAD_LEFT?SDL_GAMEPAD_BUTTON_DPAD_RIGHT:SDL_GAMEPAD_BUTTON_DPAD_LEFT);
        assert(modern_options_state(index,&after) && after.value==before.value);
        check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==action+2);
        check_pad_frame(option_captures[tab]);
    }
    options_tab=3;key_gamepad=true;pad_options_page=0;
    check_pad_frame("gamepad-options-controls-1280x800");
    check_pad_tap(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);assert(pad_options_page==1);
    check_pad_frame("gamepad-options-settings-1280x800");
    check_pad_focus_action(463);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==464);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(!gamepad.rumble);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(gamepad.rumble);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_DOWN);assert(pad_focus_nodes[pad_focus_index].action==466);
    int deadzone_before=gamepad.deadzone;check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_RIGHT);assert(gamepad.deadzone==deadzone_before+1);
    check_pad_tap(SDL_GAMEPAD_BUTTON_DPAD_LEFT);assert(gamepad.deadzone==deadzone_before);
    gamepad.deadzone=22;gamepad.cursor_speed=125;pad_save();gamepad.deadzone=5;pad_load();assert(gamepad.deadzone==22 && gamepad.cursor_speed==125);
    assert(SDL_SetWindowSize(window,800,600));text_scale=150;check_pad_frame("gamepad-options-settings-800x600-150");
    assert(SDL_SetWindowSize(window,1280,800));text_scale=100;
    mainmenu_ui_close(false);check_pad_frame(NULL);
    for(int i=0;i<2;i++) {
        mainmenu_ui_window_type_set(i?MM_WINDOW_SAVE_GAME:MM_WINDOW_LOAD_GAME);mainmenu_ui_open();check_pad_frame(NULL);
        check_pad_frame(i?"gamepad-save-1280x800":"gamepad-load-1280x800");
        assert(pad_focus_count>0);mainmenu_ui_close(false);check_pad_frame(NULL);
    }
    assert(SDL_DetachVirtualJoystick(id));check_pad_frame(NULL);assert(!gamepad.device && !follow_walk.pc);
    puts("PASS: controller menu rows and values, character points and skills, main/pause menu haptics, R3 target cycling and A attack, item/spell/skill bindings, map pan/zoom, native aim cursor and movement, scripted scenery, Y outside PC turn, menu wheels, cancellation, damage rumble and hot unplug");
}
