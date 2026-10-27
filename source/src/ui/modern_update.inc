/* One asynchronous check per launch. The bundled helper handles HTTPS and JSON. */
#ifdef SDL_PLATFORM_WINDOWS
#define UPDATE_HELPER "ArcanumModern/uap/uap-helper.exe"
#else
#define UPDATE_HELPER "ArcanumModern/uap/uap-helper"
#endif
static SDL_Process* update_process;
static bool update_started;
static Uint64 update_deadline;
static char update_tag[65];

static void update_close(void)
{
    if(update_process) {
        int code;
        if(!SDL_WaitProcess(update_process,false,&code) && SDL_KillProcess(update_process,true))
            SDL_WaitProcess(update_process,true,&code);
        SDL_DestroyProcess(update_process);update_process=NULL;
    }
    update_tag[0]=0;
}

static bool update_valid_tag(const char* text,size_t size)
{
    if(!size || size>64)return false;
    size_t i=text[0]=='v'?1:0;
    for(int part=0;part<3;part++) {
        size_t start=i;
        while(i<size && text[i]>='0' && text[i]<='9')i++;
        if(i==start)return false;
        if(part<2 && (i==size || text[i++]!='.'))return false;
    }
    return i==size;
}

static void update_tick(void)
{
    /* The engine creates a preview character even before loading a game. */
    if(in_game || !mainmenu_ui_is_active()){update_close();return;}
    bool menu=mainmenu_ui_is_active() && mainmenu_ui_window_type_get()==MM_WINDOW_MAINMENU;
    if(!update_started && menu) {
        update_started=true;
        if(!SDL_GetPathInfo(UPDATE_HELPER,NULL))return;
        char* cwd=SDL_GetCurrentDirectory();char* exe=NULL;
        if(!cwd)return;
        SDL_asprintf(&exe,"%s%s",cwd,UPDATE_HELPER);SDL_free(cwd);
        if(!exe)return;
        const char* args[]={exe,"--mode","check-update","--current-version",ARCANUM_RENAISSANCE_VERSION,NULL};
        SDL_PropertiesID props=SDL_CreateProperties();
        SDL_SetPointerProperty(props,SDL_PROP_PROCESS_CREATE_ARGS_POINTER,(void*)args);
        SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDIN_NUMBER,SDL_PROCESS_STDIO_NULL);
        SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER,SDL_PROCESS_STDIO_APP);
        SDL_SetNumberProperty(props,SDL_PROP_PROCESS_CREATE_STDERR_NUMBER,SDL_PROCESS_STDIO_NULL);
        update_process=SDL_CreateProcessWithProperties(props);
        SDL_DestroyProperties(props);SDL_free(exe);update_deadline=SDL_GetTicks()+6000;
    }
    if(update_process) {
        int code=1;
        if(SDL_WaitProcess(update_process,false,&code)) {
            size_t size=0;char* output=SDL_ReadProcess(update_process,&size,&code);
            SDL_DestroyProcess(update_process);update_process=NULL;
            if(output) {
                while(size && (output[size-1]=='\n' || output[size-1]=='\r'))size--;
                if(code==0 && update_valid_tag(output,size)) {
                    SDL_memcpy(update_tag,output,size);update_tag[size]=0;
                }
                SDL_free(output);
            }
        } else if(SDL_GetTicks()>=update_deadline)update_close();
    }
    /* Never interrupt a loaded game, character creation, or another dialog. */
    if(!menu || !*update_tag || slide_ui_is_active() || confirming_menu || menu_suspended
        || pending_action>=0 || click_captured || pressed_action>=0)return;
    char message[768],url[192];
    SDL_snprintf(message,sizeof(message),tr(
        "Arcanum Renaissance %s est disponible.\nVersion installée : %s.\n\nOuvrir la page de la release pour télécharger la mise à jour ?",
        "Arcanum Renaissance %s is available.\nInstalled version: %s.\n\nOpen the release page to download the update?"),update_tag,ARCANUM_RENAISSANCE_VERSION);
    SDL_snprintf(url,sizeof(url),"https://github.com/cdomergue/arcanum-modern/releases/tag/%s",update_tag);
    update_tag[0]=0;
    SDL_Window* parent=NULL;tig_video_window_get(&parent);
    const SDL_MessageBoxButtonData buttons[]={
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT|SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT,0,tr("Ignorer","Ignore")},
        {0,1,tr("Voir la mise à jour","View update")}
    };
    SDL_MessageBoxData question={SDL_MESSAGEBOX_INFORMATION,parent,"Arcanum Renaissance",message,2,buttons,NULL};
    int choice=0;
    if(SDL_ShowMessageBox(&question,&choice) && choice==1)SDL_OpenURL(url);
}
