#include "game/modern_autosave.h"

#include <stdio.h>
#include <string.h>

#include "game/combat.h"
#include "game/critter.h"
#include "game/gamelib.h"
#include "game/map.h"
#include "game/mes.h"
#include "game/player.h"
#include "game/teleport.h"
#include "game/ui.h"
#include "ui/dialog_ui.h"
#include "ui/intgame.h"
#include "ui/mainmenu_ui.h"
#include "ui/wmap_ui.h"

#define AUTOSAVE_GENERATIONS (36 * 36 * 36)
#define AUTOSAVE_KEEP 3

static bool autosave_pending;
static bool autosave_running;
static tig_timestamp_t autosave_requested_at;
static const char autosave_digits[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char autosave_stage[] = "MdrnAuto";
static const char* autosave_extensions[] = { ".tfaf", ".tfai", ".bmp" };

static int autosave_generation(const char* name)
{
    int generation = 0;
    if (strlen(name) < 8 || strncmp(name, "SlotA", 5) != 0) return -1;
    for (int index = 5; index < 8; index++) {
        const char* digit = strchr(autosave_digits, name[index]);
        if (digit == NULL) return -1;
        generation = generation * 36 + (int)(digit - autosave_digits);
    }
    return generation;
}

bool modern_autosave_is_slot(const char* name)
{
    return name != NULL && autosave_generation(name) >= 0;
}

static bool autosave_newer(int a, int b)
{
    int distance = (a - b + AUTOSAVE_GENERATIONS) % AUTOSAVE_GENERATIONS;
    return distance > 0 && distance < AUTOSAVE_GENERATIONS / 2;
}

static void autosave_name(int generation, char name[9])
{
    memcpy(name, "SlotA000", 9);
    for (int index = 7; index >= 5; index--) {
        name[index] = autosave_digits[generation % 36];
        generation /= 36;
    }
}

static bool autosave_safe(void)
{
    int64_t pc;
    if (autosave_running || gamelib_in_reset() || gamelib_in_load() || gamelib_in_save()
        || mainmenu_ui_is_active() || intgame_mode_get() != INTGAME_MODE_MAIN
        || wmap_ui_is_created() || dialog_ui_is_local_pc_in_dialog()
        || teleport_is_teleporting() || combat_turn_based_is_active()) return false;
    pc = player_get_local_pc_obj();
    return pc != OBJ_HANDLE_NULL && map_current_map() >= 0
        && !critter_is_dead(pc) && !critter_is_unconscious(pc)
        && !combat_critter_is_combat_mode_active(pc);
}

static void autosave_remove_stage(void)
{
    TigFileList files;
    tig_file_list_create(&files, "save\\MdrnAuto*");
    for (unsigned int index = 0; index < files.count; index++) {
        char path[TIG_MAX_PATH];
        snprintf(path, sizeof(path), "save\\%s", files.entries[index].path);
        tig_file_remove(path);
    }
    tig_file_list_destroy(&files);
}

static void autosave_prune(int newest)
{
    GameSaveList saves;
    gamelib_savelist_create(&saves);
    for (unsigned int index = 0; index < saves.count; index++) {
        int generation = autosave_generation(saves.names[index]);
        if (generation < 0) continue;
        int newer_count = 0;
        int age = (newest - generation + AUTOSAVE_GENERATIONS) % AUTOSAVE_GENERATIONS;
        for (unsigned int other = 0; other < saves.count; other++) {
            int other_generation = autosave_generation(saves.names[other]);
            if (other_generation >= 0
                && (newest - other_generation + AUTOSAVE_GENERATIONS) % AUTOSAVE_GENERATIONS < age) {
                newer_count++;
            }
        }
        if (newer_count >= AUTOSAVE_KEEP && !gamelib_delete(saves.names[index])) {
            tig_debug_printf("modern_autosave: could not remove %s\n", saves.names[index]);
        }
    }
    gamelib_savelist_destroy(&saves);
}

static bool autosave_write(void)
{
    GameSaveList saves;
    int newest = -1;
    int generation;
    char name[9];
    char description[96];
    char source[TIG_MAX_PATH];
    char destination[TIG_MAX_PATH];
    MesFileEntry label = { 0 };
    bool success = false;

    gamelib_savelist_create(&saves);
    for (unsigned int index = 0; index < saves.count; index++) {
        int candidate = autosave_generation(saves.names[index]);
        if (candidate >= 0 && (newest < 0 || autosave_newer(candidate, newest))) newest = candidate;
    }
    gamelib_savelist_destroy(&saves);
    generation = (newest + 1) % AUTOSAVE_GENERATIONS;
    for (int attempts = 0; attempts < AUTOSAVE_GENERATIONS; attempts++) {
        TigFileList files;
        autosave_name(generation, name);
        snprintf(source, sizeof(source), "save\\%s*", name);
        tig_file_list_create(&files, source);
        bool available = files.count == 0;
        tig_file_list_destroy(&files);
        if (available) break;
        generation = (generation + 1) % AUTOSAVE_GENERATIONS;
        if (attempts == AUTOSAVE_GENERATIONS - 1) return false;
    }

    label.num = 5056;
    mes_get_msg(mainmenu_ui_mes_file(), &label);
    if (label.str == NULL || label.str[0] == '\0') return false;
    snprintf(description, sizeof(description), "%.80s %s", label.str, name + 5);
    for (char* ch = description; *ch != '\0'; ch++) {
        if (strchr("\\/:*?\"<>|", *ch) != NULL || (unsigned char)*ch < 32) *ch = ' ';
    }

    autosave_running = true;
    autosave_remove_stage();
    if (!gamelib_save(autosave_stage, description)) goto done;
    for (unsigned int index = 0; index < sizeof(autosave_extensions) / sizeof(autosave_extensions[0]); index++) {
        snprintf(source, sizeof(source), "save\\%s%s", autosave_stage, autosave_extensions[index]);
        snprintf(destination, sizeof(destination), "save\\%s%s", name, autosave_extensions[index]);
        if (tig_file_rename(source, destination) != 0) goto done;
    }
    // Publishing the metadata last keeps incomplete archives out of the load menu.
    snprintf(source, sizeof(source), "save\\%s%s.gsi", autosave_stage, description);
    snprintf(destination, sizeof(destination), "save\\%s%s.gsi", name, description);
    if (tig_file_rename(source, destination) != 0) goto done;
    success = true;
    autosave_prune(generation);

done:
    if (!success) {
        for (unsigned int index = 0; index < sizeof(autosave_extensions) / sizeof(autosave_extensions[0]); index++) {
            snprintf(destination, sizeof(destination), "save\\%s%s", name, autosave_extensions[index]);
            tig_file_remove(destination);
        }
        tig_debug_printf("modern_autosave: save failed; existing saves preserved\n");
    }
    autosave_remove_stage();
    autosave_running = false;
    return success;
}

void modern_autosave_reset(void)
{
    autosave_pending = false;
}

void modern_autosave_after_combat(void)
{
    if (gamelib_in_reset() || gamelib_in_load() || gamelib_in_save() || teleport_is_teleporting()) return;
    autosave_pending = true;
    tig_timer_now(&autosave_requested_at);
}

void modern_autosave_before_map_change(void)
{
    if (autosave_safe()) {
        autosave_pending = false;
        autosave_write();
    }
}

void modern_autosave_tick(void)
{
    if (autosave_pending && tig_timer_elapsed(autosave_requested_at) >= 1000 && autosave_safe()) {
        autosave_pending = false;
        autosave_write();
    }
}
