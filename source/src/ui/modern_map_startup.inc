bool modern_ui_prepare_maps(const char* executable)
{
    if (!enabled) return true;
    SDL_Process* process = modern_maps_start(executable);
    ModernMapProgress progress = {0};
    bool failed = !process, running = process != NULL, proceed = true, finished = false;
    int old_w, old_h, exit_code = 0;
    SDL_RendererLogicalPresentation old_mode;
    SDL_BlendMode old_blend;
    SDL_Texture* old_target = SDL_GetRenderTarget(renderer);
    SDL_SetRenderTarget(renderer, NULL);
    SDL_GetRenderLogicalPresentation(renderer, &old_w, &old_h, &old_mode);
    SDL_GetRenderDrawBlendMode(renderer, &old_blend);
    bool cursor_visible = SDL_CursorVisible();
    SDL_ShowCursor();
    while (!finished) {
        if (running) {
            modern_maps_progress(&progress);
            if (SDL_WaitProcess(process, false, &exit_code)) {
                running = false;
                failed = exit_code != 0 || progress.stage != 2;
                if (!failed) break;
            }
        }
        update_geometry();
        SDL_SetRenderLogicalPresentation(renderer, (int)canvas_w, (int)canvas_h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        float cx = canvas_w / 2, cy = canvas_h / 2;
        SDL_FRect button = {cx - 160, cy + 142, 320, 48};
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {proceed = false; finished = true;}
            if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat
                && (event.key.scancode == SDL_SCANCODE_ESCAPE || event.key.scancode == SDL_SCANCODE_RETURN)) finished = true;
            if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT) {
                float x, y;
                SDL_RenderCoordinatesFromWindow(renderer, event.button.x, event.button.y, &x, &y);
                if (inside(button, x, y)) finished = true;
            }
        }
        draw_menu_background();
        SDL_FRect panel = {cx - 430, cy - 185, 860, 410};
        fill(panel, (SDL_Color){16,12,9,242}); outline(panel, MUTED);
        centered(1, tr("Préparation des cartes HD", "Preparing HD maps"), cx, cy - 145, GOLD);
        centered(0, failed
            ? tr("La préparation n’a pas pu se terminer.", "Preparation could not be completed.")
            : tr("Les cartes sont créées à partir de votre installation du jeu.", "Maps are generated from your installed game files."), cx, cy - 88, GOLD);
        centered(0, tr("Cette étape peut prendre du temps et utiliser environ 8 Go.", "This can take a while and use about 8 GB of disk space."), cx, cy - 50, MUTED);
        SDL_FRect track = {cx - 350, cy + 5, 700, 22};
        fill(track, (SDL_Color){7,6,4,255}); outline(track, MUTED);
        double fraction = progress.total ? (double)progress.done / progress.total : 0;
        SDL_FRect bar = {track.x + 2, track.y + 2, (track.w - 4) * (float)fraction, track.h - 4};
        if (!progress.total && !failed) {
            bar.w = 100; bar.x += (float)((SDL_GetTicks() / 8) % 594);
        }
        fill(bar, GOLD);
        char status[160];
        if (progress.total) SDL_snprintf(status, sizeof(status), "%d%%  —  %llu / %llu", (int)(fraction * 100), progress.done, progress.total);
        else SDL_strlcpy(status, tr("Vérification et préparation…", "Checking and preparing…"), sizeof(status));
        centered(0, status, cx, cy + 43, GOLD);
        centered(0, tr("Vous pouvez jouer maintenant et reprendre au prochain lancement.", "You can play now and resume at the next launch."), cx, cy + 94, MUTED);
        fill(button, (SDL_Color){54,38,21,255}); outline(button, GOLD);
        centered(0, tr("Jouer maintenant", "Play now"), cx, button.y + 13, GOLD);
        SDL_RenderPresent(renderer);
        SDL_Delay(33);
    }
    if (running) {SDL_KillProcess(process, true); SDL_WaitProcess(process, true, &exit_code);}
    if (process) SDL_DestroyProcess(process);
    if (!cursor_visible) SDL_HideCursor();
    SDL_SetRenderDrawBlendMode(renderer, old_blend);
    SDL_SetRenderLogicalPresentation(renderer, old_w, old_h, old_mode);
    SDL_SetRenderTarget(renderer, old_target);
    return proceed;
}
