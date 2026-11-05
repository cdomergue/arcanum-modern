/* Included by the native UI harness; all writes stay in its marked fixture. */
static void check_portrait_in_game(int64_t pc,const char* mode)
{
    if(!strcmp(mode,"reload")) {
        int expected=0;FILE* f=fopen("portrait-test-id.txt","r");assert(f);
        assert(fscanf(f,"%d",&expected)==1);fclose(f);
        assert(portrait_get(pc)==expected);
        char path[260];portrait_path(expected,path,sizeof(path),128);
        SDL_IOStream* stream=tig_file_io_open(path,"rb");assert(stream);
        SDL_Surface* bmp=SDL_LoadBMP_IO(stream,true);assert(bmp && bmp->w==128 && bmp->h==128);
        SDL_DestroySurface(bmp);
        assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));capture_frame("portrait-reloaded");charedit_close();
        puts("PASS: changed portrait and imported image survive engine restart and save loading");return;
    }
    int old=portrait_get(pc),strength=stat_base_get(pc,STAT_STRENGTH);
    assert(charedit_open(pc,CHAREDIT_MODE_ACTIVE));
    click_action(3000);assert(portrait_editor.active && portrait_editor.choosing);
    capture_frame("portrait-choose-100");
    click_action(3002);assert(portrait_editor.selected!=old && portrait_get(pc)==old);
    /* A held Escape cancels this modal without closing the character sheet. */
    SDL_Event event;SDL_zero(event);event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_ESCAPE;
    assert(handle_event(&event));modern_ui_tick();SDL_Delay(180);
    event.key.repeat=true;assert(handle_event(&event));
    event.type=SDL_EVENT_KEY_UP;event.key.repeat=false;assert(handle_event(&event));
    assert(!portrait_editor.active && play_sheet() && portrait_get(pc)==old);
    click_action(3000);click_action(3002);int chosen=portrait_editor.selected;
    click_action(515);assert(portrait_get(pc)==chosen && !portrait_editor.active);
    click_action(3000);text_scale=150;capture_frame("portrait-choose-150");
    /* Feed the same callback result as the native picker, without an OS dialog. */
    portrait_editor.choosing=false;
    portrait_picker.path=SDL_strdup("modern-ui/portrait-test.png");
    portrait_picker.failed=false;SDL_SetAtomicInt(&portrait_picker.state,2);
    capture_frame("portrait-crop-150");assert(portrait_editor.pixels);
    click_action(517);assert(portrait_editor.zoom>1);
    click_action(514);assert(portrait_get(pc)==chosen);
    click_action(3000);portrait_editor.choosing=false;
    portrait_picker.path=SDL_strdup("modern-ui/portrait-test.png");
    portrait_picker.failed=false;SDL_SetAtomicInt(&portrait_picker.state,2);
    capture_frame(NULL);assert(portrait_editor.pixels);
    click_action(515);int imported=portrait_get(pc);
    assert(imported>0 && imported<1000 && imported!=chosen && !portrait_editor.active);
    assert(stat_base_get(pc,STAT_STRENGTH)==strength);
    capture_frame("portrait-applied");
    /* Leaving the sheet discards any pending choice. */
    click_action(3000);charedit_close();modern_ui_tick();assert(!portrait_editor.active);
    assert(portrait_get(pc)==imported);
    int64_t followers[128];int count=modern_followers_get(followers,128);
    if(count){
        assert(charedit_open(followers[0],CHAREDIT_MODE_ACTIVE));
        portrait_editor_action(3000);assert(!portrait_editor.active);charedit_close();
    }
    assert(gamelib_save("SlotPort","Portrait change regression fixture"));
    FILE* f=fopen("portrait-test-id.txt","w");assert(f);fprintf(f,"%d\n",imported);fclose(f);
    puts("PASS: portrait preview, cancel, held Escape, selection, crop, import, hero-only scope and save");
}
