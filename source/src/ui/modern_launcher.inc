/* Interactive desktop launches go through the updater. This is a routing
 * convention, not authentication: offline and skipped updates remain valid. */
#ifdef SDL_PLATFORM_WINDOWS
#include <windows.h>
#endif

static int modern_launcher_entry(int* argc, char** argv)
{
#if defined(SDL_PLATFORM_WINDOWS) || defined(SDL_PLATFORM_LINUX) || defined(SDL_PLATFORM_MACOS)
    bool from_launcher = false;
    for (int i = 1; i < *argc;) {
        if (strcmp(argv[i], "--renaissance-launcher") != 0) { i++; continue; }
        from_launcher = true;
        for (int j = i; j < *argc; j++) argv[j] = argv[j + 1];
        (*argc)--;
    }
    if (from_launcher) return -1;
    /* Noninteractive diagnostics and the separately marked editor playtest
     * are internal entry points, not ways of starting a normal game. */
    if ((*argc == 2 && strcmp(argv[1], "--check-movies") == 0)
        || (*argc >= 3 && strcmp(argv[1], "--modern-generate-maps") == 0)
        || (*argc == 5 && strcmp(argv[1], "--worlded-test") == 0)) return -1;

    const char* base = SDL_GetBasePath();
    char* executable = NULL;
#ifdef SDL_PLATFORM_WINDOWS
    if (base) SDL_asprintf(&executable, "%srenaissance-launcher.exe", base);
#else
    if (base) SDL_asprintf(&executable, "%srenaissance-launcher", base);
#endif
    const char** args = SDL_calloc((size_t)*argc + 4, sizeof(*args));
    SDL_Process* process = NULL;
    if (executable && args) {
        char parent[32];
    #ifdef SDL_PLATFORM_WINDOWS
        SDL_snprintf(parent, sizeof(parent), "%lu", (unsigned long)GetCurrentProcessId());
#else
        SDL_snprintf(parent, sizeof(parent), "%lu", (unsigned long)getpid());
#endif
        args[0] = executable;
        args[1] = "--launcher-redirect-parent";
        args[2] = parent;
        args[3] = "--";
        for (int i = 1; i < *argc; i++) args[i + 3] = argv[i];
        SDL_PropertiesID props = SDL_CreateProperties();
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
        SDL_SetStringProperty(props, SDL_PROP_PROCESS_CREATE_WORKING_DIRECTORY_STRING, base);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_BACKGROUND_BOOLEAN, true);
        process = SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);
    }
    SDL_free((void*)args);
    SDL_free(executable);
    if (!process) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcanum Renaissance",
            "Unable to open the Renaissance launcher. Reinstall the complete Renaissance package.\n"
            "Impossible d'ouvrir le lanceur Renaissance. Reinstallez le package Renaissance complet.", NULL);
        return EXIT_FAILURE;
    }
    SDL_DestroyProcess(process);
    return EXIT_SUCCESS;
#else
    (void)argc;
    (void)argv;
    return -1;
#endif
}
