#include "game/modern_uap.h"
#include <tig/tig.h>
#include "game/gamelib.h"

#define UAP_ROOT_FR "modern-ui/uap-fr/active/"
#define UAP_ROOT_EN "modern-ui/uap-en/active/"
static char module_paths[3][TIG_MAX_PATH];
static bool disabled;

static const char* active_root(void)
{
    const char* roots[] = {UAP_ROOT_FR, UAP_ROOT_EN};
    const char* versions[] = {"2.0.2-fr-1\n", "2.0.2-en-1\n"};
    for (size_t i = 0; i < SDL_arraysize(roots); i++) {
        char path[TIG_MAX_PATH], marker[32] = {0};
        snprintf(path, sizeof(path), "%sready", roots[i]);
        FILE* file = fopen(path, "rb");
        if (!file) continue;
        size_t size = fread(marker, 1, sizeof(marker) - 1, file);
        fclose(file);
        if (size == strlen(versions[i]) && !strcmp(marker, versions[i])) return roots[i];
    }
    return NULL;
}

bool modern_uap_ready(void)
{
    return !disabled && active_root() && !tig_file_exists("Arcanum5.dat", NULL);
}

bool modern_uap_load(void)
{
    if (!modern_uap_ready()) return true;
    const char* root = active_root();
    bool french = !strcmp(root, UAP_ROOT_FR);
    const char* archives[] = {
        "global/arcanum5.dat", "global/arcanumxambient.dat",
        "global/arcanumxanims1base.dat", "global/arcanumxanims2dupes.dat",
        "global/arcanumxanims3uap.dat", "global/arcanumxraces.dat",
        "french-global.dat", "data"
    };
    for (size_t i = 0; i < SDL_arraysize(archives); i++) {
        if (!french && !strcmp(archives[i], "french-global.dat")) continue;
        char path[TIG_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", root, archives[i]);
        if (!tig_file_repository_add(path)) {
            tig_debug_printf("Cannot mount UAP overlay: %s\n", path);
            for (size_t j = 0; j < i; j++) {
                if (!french && !strcmp(archives[j], "french-global.dat")) continue;
                snprintf(path, sizeof(path), "%s%s", root, archives[j]);
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
    const char* root = active_root();
    const char* files[3] = {0};
    if (!SDL_strcasecmp(module, "Arcanum")) {
        files[0] = "modules/arcanum.patch1";
        files[1] = "modules/arcanum.patch9";
        if (!strcmp(root, UAP_ROOT_FR)) files[2] = "french-module.dat";
    } else if (!SDL_strcasecmp(module, "Vormantown")) {
        files[0] = "modules/vormantown.patch9";
    }
    for (int i = 0; i < 3 && files[i]; i++) {
        char path[TIG_MAX_PATH];
        snprintf(path, sizeof(path), "%s%s", root, files[i]);
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
