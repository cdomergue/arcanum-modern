/* Exercise the live footer together with the existing top-bar transitions. */
#define main top_bar_test_main
#include "check-top-bar.c"
#undef main

static void capture_footer(const char* prefix,const char* name)
{
    char path[4096];snprintf(path,sizeof(path),"%s-%s.png",prefix,name);render_check(path);
}

static void check_footer_scales(const char* prefix)
{
    const int sizes[][2]={{1600,900},{1366,768},{1920,1080},{1280,720},{1173,659}};
    const float zooms[]={.75f,.9f,1.f,1.12f,1.4f};
    float previous_zoom=world_zoom;
    for(int i=0;i<5;i++) {
        SDL_SetWindowSize(window,sizes[i][0],sizes[i][1]);
        for(int z=0;z<5;z++) {
            world_zoom=zooms[z];zoom_pending=true;render_check(NULL);
            char name[80];snprintf(name,sizeof(name),"scale-%dx%d-zoom-%d",sizes[i][0],sizes[i][1],z);
            capture_footer(prefix,name);
            SDL_Surface* pixels=SDL_RenderReadPixels(renderer,NULL);assert(pixels);
            /* Every interior column of the actual liquid must be painted,
             * including at fractional world/UI scales. */
            for(int fatigue=0;fatigue<2;fatigue++) {
                SDL_FRect glass=bottom_bar_rect(fatigue?1957:47,90,72,238);
                float sx=pixels->w/canvas_w,sy=pixels->h/canvas_h;
                int y=(int)((glass.y+glass.h*.9f)*sy);
                for(int x=(int)ceilf(glass.x*sx)+2;x<(int)floorf((glass.x+glass.w)*sx)-2;x++) {
                    Uint8 r,g,b,a;assert(SDL_ReadSurfacePixel(pixels,x,y,&r,&g,&b,&a));
                    assert(fatigue?(b>=80 && b>r):(r>=60 && r>g));
                }
            }
            SDL_DestroySurface(pixels);
        }
    }
    world_zoom=previous_zoom;zoom_pending=true;
    SDL_SetWindowSize(window,1600,900);render_check(NULL);
}

static int check_bottom_bar(int argc,char** argv)
{
    int result=top_bar_test_main(argc,argv);if(result)return result;
    fixture_count=0;SDL_SetWindowSize(window,1600,900);update_geometry();
    assert(bottom_bar_texture);
    int64_t pc=player_get_local_pc_obj();
    message_text[0]=0;SDL_WarpMouseInWindow(window,0,0);
    assert(modern_hotkey_assign(0,HOTKEY_SKILL,1,0));
    assert(modern_hotkey_assign(10,HOTKEY_SKILL,2,0));
    capture_footer(argv[3],"normal");
    check_footer_scales(argv[3]);
    SDL_FRect bounds=bottom_bar_rect(0,0,BOTTOM_BAR_SOURCE_WIDTH,BOTTOM_BAR_SOURCE_HEIGHT);
    for(int action=300;action<310;action++) {
        SDL_FRect r=action_rect(action);
        assert(r.x>=bounds.x && r.x+r.w<=bounds.x+bounds.w);
        assert(hud_region(r.x+r.w/2,r.y+r.h/2));
    }
    /* Native round controls, with a real held click and deferred release. */
    held_click(6);assert(selector==1);capture_footer(argv[3],"spell-panel");held_click(840);
    held_click(5);assert(selector==2);capture_footer(argv[3],"skill-panel");held_click(840);
    held_click(8);assert(native_panel()==7);capture_footer(argv[3],"schematic-panel");held_click(929);
    held_click(300);assert(intgame_mode_get()==INTGAME_MODE_SKILL);intgame_mode_set(INTGAME_MODE_MAIN);
    SDL_Event shift={0};shift.type=SDL_EVENT_KEY_DOWN;shift.key.scancode=SDL_SCANCODE_LSHIFT;shift.key.mod=SDL_KMOD_LSHIFT;
    handle_event(&shift);SDL_Delay(180);assert(hotbar_offset()==10);
    capture_footer(argv[3],"shift");held_click(310);assert(intgame_mode_get()==INTGAME_MODE_SKILL);
    intgame_mode_set(INTGAME_MODE_MAIN);shift.type=SDL_EVENT_KEY_UP;shift.key.mod=0;handle_event(&shift);
    assert(hotbar_offset()==0);
    SDL_strlcpy(message_text,"Mine dangereuse jonchée de squelettes et de veuves noires.",sizeof(message_text));
    message_time=SDL_GetTicks();assert(!hud_message_expanded());capture_footer(argv[3],"message");
    SDL_strlcpy(message_text,"Une longue description accentuée doit rester lisible entièrement. Les messages du jeu conservent leur retour à la ligne et leur contenu, même lorsqu’ils dépassent la hauteur du panneau central. Cette vérification couvre aussi les caractères é, è et à.",sizeof(message_text));
    message_time=SDL_GetTicks();assert(hud_message_expanded());capture_footer(argv[3],"long-message");
    message_text[0]=0;
    SDL_FRect glass=bottom_bar_rect(47,90,72,238);
    SDL_WarpMouseInWindow(window,glass.x+glass.w/2,glass.y+glass.h/2);
    capture_footer(argv[3],"hp-tooltip");
    SDL_WarpMouseInWindow(window,0,0);render_check(NULL);
    /* Use the actual native examination capture, including translated fields. */
    assert(party_count>=3);int64_t inspected=party_members[2];char name[MAX_STRING];
    object_examine(inspected,pc,name);object_hover_obj_set(inspected);
    assert(modern_examine_begin(pc,inspected));
    intgame_examine_object(pc,inspected,name);modern_examine_end();
    assert(modern_hover_details() && modern_hover_details()->bar_count==2);
    capture_footer(argv[3],"hover");assert(world_hover_rect.h==0);
    assert(modern_hover_details());
    SDL_SetWindowSize(window,1024,768);update_geometry();render_check(NULL);
    object_hover_obj_set(inspected);capture_footer(argv[3],"narrow-hover");
    assert(modern_hover_details());
    object_hover_obj_set(OBJ_HANDLE_NULL);
    SDL_SetWindowSize(window,1024,768);update_geometry();capture_footer(argv[3],"narrow");
    held_click(6);assert(selector==1);held_click(840);
    SDL_SetWindowSize(window,1600,900);update_geometry();render_check(NULL);
    held_click(7);
    for(int i=0;i<45;i++){modern_ui_tick();SDL_Delay(25);}
    assert(combat_critter_is_combat_mode_active(pc));capture_footer(argv[3],"combat");
    puts("PASS: bottom HUD hitboxes, native controls, both held-Shift banks, messages, resource values and combat transition");
    return 0;
}

#ifndef ARCANUM_CHECK_BOTTOM_EMBEDDED
int main(int argc,char** argv){return check_bottom_bar(argc,argv);}
#endif
