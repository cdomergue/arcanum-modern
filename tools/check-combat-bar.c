/* Exercise the actual combat controls and native panels on an isolated save. */
#define ARCANUM_CHECK_BOTTOM_EMBEDDED
#include "check-bottom-bar.c"

static void combat_bar_settle(void)
{
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    SDL_WarpMouseInWindow(window,0,0);render_check(NULL);
}

int main(int argc,char** argv)
{
    int result=check_bottom_bar(argc,argv);if(result)return result;
    int64_t pc=player_get_local_pc_obj();
    combat_companion_ai_set(false);combat_companion_ai_sync();combat_bar_settle();
    assert(tactical_active() && combat_input_ready(pc));
    message_text[0]=0;capture_footer(argv[3],"combat-ready");
    SDL_FRect frame=bottom_bar_rect(0,0,BOTTOM_BAR_SOURCE_WIDTH,BOTTOM_BAR_SOURCE_HEIGHT);
    const int commands[]={1103,5,6,1104,1105,1109,1106,1100};
    for(int i=0;i<8;i++) {
        SDL_FRect r=action_rect(commands[i]);assert(inside(frame,r.x+r.w/2,r.y+r.h/2));
        assert(tactical_region(r.x+r.w/2,r.y+r.h/2));
    }
    for(int i=0;i<10;i++)assert(action_rect(300+i).y==action_rect(300).y);
    int points=combat_action_points_get();
    held_click(1105);assert(play_sheet());capture_footer(argv[3],"combat-character");held_click(720);
    held_click(1104);assert(inven_ui_is_created());capture_footer(argv[3],"combat-inventory");held_click(730);
    held_click(6);assert(selector==1);held_click(840);
    held_click(5);assert(selector==2);held_click(840);
    assert(combat_action_points_get()==points);
    held_click(1106);assert(tactical_log_open);capture_footer(argv[3],"combat-log");
    SDL_FRect log=tactical_log_rect();assert(log.y+log.h<frame.y);held_click(1106);assert(!tactical_log_open);
    SDL_Event shift={0};shift.type=SDL_EVENT_KEY_DOWN;shift.key.scancode=SDL_SCANCODE_LSHIFT;shift.key.mod=SDL_KMOD_LSHIFT;
    handle_event(&shift);SDL_Delay(180);capture_footer(argv[3],"combat-shift");
    for(int i=0;i<10;i++)action_rect(310+i);
    held_click(310);assert(intgame_mode_get()==INTGAME_MODE_SKILL);intgame_mode_set(INTGAME_MODE_MAIN);
    shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);assert(hotbar_offset()==0);
    held_click(1109);assert(combat_companion_ai_get());combat_companion_ai_sync();
    capture_footer(argv[3],"combat-auto");held_click(1109);combat_companion_ai_sync();assert(!combat_companion_ai_get());
    /* The current initiative segment may end at an enemy; start an actual
     * companion turn to verify that every control follows the active actor. */
    assert(party_count>0);int64_t companion=party_members[0];
    combat_turn_based_whos_turn_set(companion);combat_bar_settle();
    assert(combat_input_actor()==companion && tactical_actor==companion);
    capture_footer(argv[3],"combat-companion");held_click(1104);
    ModernInventory inventory;assert(modern_inventory_state(&inventory) && inventory.pc==companion);held_click(730);
    combat_turn_based_whos_turn_set(pc);combat_bar_settle();
    /* Inspect a real actor through native hit testing, not a fabricated card. */
    location_origin_set(obj_field_int64_get(companion,OBJ_F_LOCATION));
    TigRect object_rect;object_get_rect(companion,0,&object_rect);bool pointed=false;
    for(int y=object_rect.y;y<object_rect.y+object_rect.height && !pointed;y+=2)
        for(int x=object_rect.x;x<object_rect.x+object_rect.width;x+=2) {
            TargetDescriptor target;
            if(target_pick_at_screen_xy_ex(x,y,&target,TGT_OBJECT|TGT_OBJ_NO_T_WALL,true) && !target.is_loc && target.obj==companion) {
                tig_mouse_set_position(x,y);int ww,wh;SDL_GetWindowSize(window,&ww,&wh);
                SDL_WarpMouseInWindow(window,x*ww/hrp_iso_window_width_get(),y*wh/hrp_iso_window_height_get());
                pointed=true;break;
            }
        }
    assert(pointed && modern_hover_details());
    capture_footer(argv[3],"combat-hover");assert(world_hover_rect.h==0);
    action_rect(1100);action_rect(1109);action_rect(1106);
    assert(combat_consume_action_points(pc,2));combat_bar_settle();
    capture_footer(argv[3],"combat-pa");
    SDL_SetWindowSize(window,1024,768);update_geometry();combat_bar_settle();
    capture_footer(argv[3],"combat-narrow");held_click(1105);assert(play_sheet());held_click(720);
    SDL_SetWindowSize(window,1600,900);update_geometry();combat_bar_settle();
    held_click(1100);combat_bar_settle();assert(combat_input_actor()!=pc || !combat_input_ready(pc));
    capture_footer(argv[3],"combat-ended");
    puts("PASS: combat HUD panels, PA preservation, single-row Shift bank, AI toggle, companion inventory, log placement, held end-turn and narrow layout");
    return 0;
}
