#include "ui/modern_uap_startup.h"
#include "ui/modern_ui.h"
#include <tig/tig.h>
#ifdef SDL_PLATFORM_MACOS
#include <sys/sysctl.h>
#endif

#define UAP_WORK_FR "modern-ui/uap-fr/"
#define UAP_WORK_EN "modern-ui/uap-en/"
#ifdef SDL_PLATFORM_WINDOWS
#define UAP_HELPER "ArcanumModern/uap/uap-helper.exe"
#else
#define UAP_HELPER "ArcanumModern/uap/uap-helper"
#endif

static SDL_Texture* label(SDL_Renderer* renderer, const char* utf8, float size)
{
    char* encoded = SDL_iconv_string("WINDOWS-1252", "UTF-8", utf8, SDL_strlen(utf8) + 1);
    SDL_Texture* texture = encoded ? modern_ui_loading_label(renderer, encoded, size) : NULL;
    SDL_free(encoded);
    return texture;
}

static void centered(SDL_Renderer* renderer, SDL_Texture* texture, float y)
{
    if (!texture) return;
    float w, h;
    SDL_GetTextureSize(texture, &w, &h);
    SDL_FRect box = {(1000 - w) / 2, y, w, h};
    SDL_SetTextureColorMod(texture, 219, 193, 128);
    SDL_RenderTexture(renderer, texture, NULL, &box);
}

static int helper(const char* mode, bool french)
{
    bool hotfix = strstr(mode, "hotfix") != NULL;
    const char* work = hotfix ? (french ? "modern-ui/uap-hotfix/fr/" : "modern-ui/uap-hotfix/en/")
        : french ? UAP_WORK_FR : UAP_WORK_EN;
    bool installing = !strcmp(mode, "install") || !strcmp(mode, "install-hotfix");
    if (!SDL_CreateDirectory(work)) return 1;
    char path[TIG_MAX_PATH];
    SDL_snprintf(path, sizeof(path), "%spreparation.log", work);
    SDL_IOStream* log = SDL_IOFromFile(path, "ab");
    char* cwd = SDL_GetCurrentDirectory();
    char* executable;
    SDL_asprintf(&executable, "%s%s", cwd, UAP_HELPER);
    const char* args[] = {executable, "--game", cwd, "--mode", mode, "--language", !strcmp(mode, "probe") ? "auto" : french ? "fr" : "en", NULL};
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_ARGS_POINTER, (void*)args);
    if (log) {
        SDL_SetNumberProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_NUMBER, SDL_PROCESS_STDIO_REDIRECT);
        SDL_SetPointerProperty(props, SDL_PROP_PROCESS_CREATE_STDOUT_POINTER, log);
        SDL_SetBooleanProperty(props, SDL_PROP_PROCESS_CREATE_STDERR_TO_STDOUT_BOOLEAN, true);
    }
    SDL_Process* process = SDL_CreateProcessWithProperties(props);
    SDL_DestroyProperties(props);
    SDL_free(executable); SDL_free(cwd);
    if (!process) {if (log) SDL_CloseIO(log); return 1;}
    SDL_Renderer* renderer = NULL;
    if (installing) tig_video_renderer_get(&renderer);
    int old_w = 0, old_h = 0, code = 1;
    SDL_RendererLogicalPresentation old_mode = SDL_LOGICAL_PRESENTATION_DISABLED;
    SDL_Texture* target = renderer ? SDL_GetRenderTarget(renderer) : NULL;
    if (renderer) {
        SDL_SetRenderTarget(renderer, NULL);
        SDL_GetRenderLogicalPresentation(renderer, &old_w, &old_h, &old_mode);
        SDL_SetRenderLogicalPresentation(renderer, 1000, 700, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    }
    const char* phase_keys[] = {"check", "download", "verify", "extract", "files", "translate", "backup", "ready", "hotfix"};
    const char* fr[] = {"Vérification du jeu", "Téléchargement de l'UAP", "Vérification du téléchargement", "Extraction de l'UAP", "Installation des données UAP", "Adaptation française", "Sauvegarde et vérification de vos parties", "Préparation terminée", "Téléchargement des correctifs UAP"};
    const char* en[] = {"Checking game files", "Downloading UAP", "Verifying download", "Extracting UAP", "Installing UAP data", "Preparing French text", "Backing up and verifying saved games", "Preparation complete", "Downloading UAP hotfixes"};
    SDL_Texture* title = renderer ? label(renderer, "Arcanum Renaissance", 38) : NULL;
    SDL_Texture* hint = renderer ? label(renderer, french ? "Échap : annuler et quitter" : "Esc: cancel and exit", 22) : NULL;
    SDL_Texture* phases[9] = {0};
    for (int i = 0; renderer && i < 9; i++) phases[i] = label(renderer, french ? fr[i] : en[i], 25);
    while (!SDL_WaitProcess(process, false, &code)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED
                || (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)) {
                SDL_KillProcess(process, true); SDL_WaitProcess(process, true, &code); code = -1; goto done;
            }
        }
        int phase = 0, percent = 0;
        if (installing) {
            char key[32] = "";
            SDL_snprintf(path, sizeof(path), "%sprogress", work);
            char* status = SDL_LoadFile(path, NULL);
            if (status) sscanf(status, "%31s %d", key, &percent);
            SDL_free(status);
            for (int i = 0; i < 9; i++) if (!strcmp(key, phase_keys[i])) phase = i;
        }
        if (renderer) {
            SDL_SetRenderDrawColor(renderer, 17, 13, 9, 255); SDL_RenderClear(renderer);
            centered(renderer, title, 220); centered(renderer, phases[phase], 305); centered(renderer, hint, 425);
            SDL_FRect track = {150, 375, 700, 20};
            SDL_SetRenderDrawColor(renderer, 55, 40, 23, 255); SDL_RenderFillRect(renderer, &track);
            SDL_FRect bar = {152, 377, 696 * SDL_clamp(percent, 0, 100) / 100.f, 16};
            if (!percent) {bar.w = 100; bar.x += (SDL_GetTicks() / 8) % 596;}
            SDL_SetRenderDrawColor(renderer, 185, 151, 81, 255); SDL_RenderFillRect(renderer, &bar);
            SDL_RenderPresent(renderer);
        }
        SDL_Delay(33);
    }
 done:
    SDL_DestroyProcess(process);
    if (log) SDL_CloseIO(log);
    SDL_DestroyTexture(title); SDL_DestroyTexture(hint);
    for (int i = 0; i < 9; i++) SDL_DestroyTexture(phases[i]);
    if (renderer) {
        SDL_SetRenderLogicalPresentation(renderer, old_w, old_h, old_mode);
        SDL_SetRenderTarget(renderer, target);
    }
    return code;
}

bool modern_uap_startup(void)
{
#ifdef SDL_PLATFORM_MACOS
    char version[64]; size_t size = sizeof(version);
    if (sysctlbyname("kern.osrelease", version, &size, NULL, 0) != 0 || SDL_atoi(version) < 22) return true;
#endif
    if (!SDL_GetPathInfo(UAP_HELPER, NULL)) return true;
    int result = helper("probe", false);
    if (result == -1) return false;
    if (result < 20 || result > 23) return true;
    SDL_Window* window = NULL;
    tig_video_window_get(&window);
    bool french = result == 20 || result == 22;
    bool hotfix = result >= 22;
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, french ? "Non" : "No"},
        {0, 1, french ? "Oui" : "Yes"}
    };
    SDL_MessageBoxData question = {SDL_MESSAGEBOX_WARNING, window, "Arcanum Renaissance — UAP",
        french ? "Installer l'UAP 2.0.2 (Base + Race Mod) et les correctifs 1.1.0 en français ?\n\n"
        "L'UAP modifie les quêtes, scripts, caractéristiques, objets et races.\n"
        "Une partie existante peut présenter des blocages ou des incompatibilités.\n\n"
        "Vos sauvegardes seront copiées et vérifiées avant toute activation.\nLes archives d'origine seront conservées.\n\n"
        "Téléchargement automatique : environ 485 Mo.\nPrévoyez 2 Go libres, plus la sauvegarde de vos parties.\n"
        "Les cartes HD devront être préparées à nouveau.\n\n"
        "Non : continuer sans UAP et mémoriser ce choix." :
        "Install UAP 2.0.2 (Base + Race Mod) and hotfixes 1.1.0 for your English game?\n\n"
        "UAP changes quests, scripts, statistics, items and races.\nExisting campaigns may encounter blocked quests or incompatibilities.\n\n"
        "Saved games will be backed up and verified before activation.\nOriginal game archives are preserved.\n\n"
        "Automatic download: about 485 MB.\nAllow 2 GB of free space plus your save backup.\nHD maps will need to be prepared again.\n\n"
        "No: continue without UAP and remember this choice.", 2, buttons, NULL};
    if (hotfix) question.message = french ?
        "Ajouter les correctifs UAP 1.1.0 en français ?\n\n"
        "Ils corrigent des dialogues, des objets et des zones.\nVotre installation UAP sera conservée.\n\n"
        "Téléchargement : moins de 1 Mo.\nVos sauvegardes seront copiées et vérifiées avant activation.\n"
        "Les cartes HD devront être préparées à nouveau.\nCertaines corrections peuvent ne pas être rétroactives dans une partie existante.\n\n"
        "Non : continuer sans ces correctifs et mémoriser ce choix.":
        "Add UAP hotfixes 1.1.0?\n\n"
        "They fix dialogues, items and areas.\nYour existing UAP installation will be preserved.\n\n"
        "Download: less than 1 MB.\nSaved games will be backed up and verified before activation.\n"
        "HD maps will need to be prepared again.\nSome fixes may not apply retroactively to existing saves.\n\n"
        "No: continue without these hotfixes and remember this choice.";
    int choice = 0;
    if (!SDL_ShowMessageBox(&question, &choice)) return true;
    result = helper(hotfix ? (choice == 1 ? "install-hotfix" : "decline-hotfix")
        : (choice == 1 ? "install" : "decline"), french);
    if (result == -1) return false;
    if (result != 0) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Arcanum Renaissance — UAP",
            hotfix ? (french ? "La préparation des correctifs n'a pas abouti. Votre UAP reste inchangé.\n\nJournal : modern-ui/uap-hotfix/fr/preparation.log" :
                "Hotfix preparation did not finish. Your UAP is unchanged.\n\nLog: modern-ui/uap-hotfix/en/preparation.log") :
            french ? "La préparation de l'UAP n'a pas abouti. Aucun patch partiel n'est activé.\n\nJournal : modern-ui/uap-fr/preparation.log" :
                "UAP preparation did not finish. No partial patch is activated.\n\nLog: modern-ui/uap-en/preparation.log", window);
    }
    return true;
}
