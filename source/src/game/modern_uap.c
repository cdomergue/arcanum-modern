#include "game/modern_uap.h"
#include <tig/tig.h>
#include "game/gamelib.h"

#define UAP_ROOT "modern-ui/uap-fr/active/"
static char module_paths[3][TIG_MAX_PATH];
static bool disabled;

bool modern_uap_ready(void)
{
    if (disabled) return false;
    char marker[32] = {0};
    FILE* file = fopen(UAP_ROOT "ready", "rb");
    if (!file) return false;
    size_t size = fread(marker, 1, sizeof(marker) - 1, file);
    fclose(file);
    return size == strlen("2.0.2-fr-1\n") && !strcmp(marker, "2.0.2-fr-1\n")
        && !tig_file_exists("Arcanum5.dat", NULL);
}

bool modern_uap_load(void)
{
    if (!modern_uap_ready()) return true;
    const char* archives[] = {
        "global/arcanum5.dat", "global/arcanumxambient.dat",
        "global/arcanumxanims1base.dat", "global/arcanumxanims2dupes.dat",
        "global/arcanumxanims3uap.dat", "global/arcanumxraces.dat",
        "french-global.dat", "data"
    };
    for (size_t i = 0; i < SDL_arraysize(archives); i++) {
        char path[TIG_MAX_PATH];
        snprintf(path, sizeof(path), UAP_ROOT "%s", archives[i]);
        if (!tig_file_repository_add(path)) {
            tig_debug_printf("Cannot mount UAP French overlay: %s\n", path);
            for (size_t j = 0; j < i; j++) {
                snprintf(path, sizeof(path), UAP_ROOT "%s", archives[j]);
                tig_file_repository_remove(path);
            }
            disabled = true;
            return false;
        }
    }
    return true;
}

bool modern_uap_module_load(const char* module)
{
    modern_uap_module_unload();
    if (!modern_uap_ready()) return true;
    const char* files[3] = {0};
    if (!SDL_strcasecmp(module, "Arcanum")) {
        files[0] = "modules/arcanum.patch1";
        files[1] = "modules/arcanum.patch9";
        files[2] = "french-module.dat";
    } else if (!SDL_strcasecmp(module, "Vormantown")) {
        files[0] = "modules/vormantown.patch9";
    }
    for (int i = 0; i < 3 && files[i]; i++) {
        char path[TIG_MAX_PATH];
        snprintf(path, sizeof(path), UAP_ROOT "%s", files[i]);
        if (!tig_file_repository_add(path)) {modern_uap_module_unload(); return false;}
        SDL_strlcpy(module_paths[i], path, sizeof(module_paths[i]));
    }
    return true;
}

void modern_uap_module_unload(void)
{
    for (int i = 2; i >= 0; i--) {
        if (*module_paths[i]) tig_file_repository_remove(module_paths[i]);
        *module_paths[i] = 0;
    }
}

TigFile* modern_uap_original_sector(const char* path)
{
    if (!modern_uap_ready()) return NULL;
    const char* module = gamelib_current_module_name_get();
    char archive[TIG_MAX_PATH];
    for (int patch = 9; patch >= -1; patch--) {
        if (patch >= 0) snprintf(archive, sizeof(archive), "./modules/%s.patch%d", module, patch);
        else snprintf(archive, sizeof(archive), "./modules/%s.dat", module);
        TigFile* stream = tig_file_fopen_repository(archive, path);
        if (stream) return stream;
    }
    return NULL;
}
