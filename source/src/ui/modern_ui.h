#ifndef ARCANUM_UI_MODERN_UI_H_
#define ARCANUM_UI_MODERN_UI_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct ModernCombatPreview {
    int64_t actor, target, location;
    int kind, cost, step_cost, chance, radius, count;
    bool reachable, affordable, fatigue, blocked, in_range;
    int64_t path[201];
} ModernCombatPreview;
bool modern_combat_preview(ModernCombatPreview* preview);
void modern_combat_sync(void);
void modern_combat_log(int64_t object, const char* text);
int modern_spell_selected(void);
bool modern_spell_resources(int64_t actor, int spell, int* fatigue_cost);
int modern_maintained_spells(int64_t actor, int* ids, int* spells, int capacity);
bool modern_spell_stop(int64_t actor, int id);

bool modern_ui_init(void);
void modern_ui_exit(void);
bool modern_ui_enabled(void);
bool modern_world_resize(int width,int height,float anchor_x,float anchor_y);
void modern_ui_tick(void);
void modern_ui_menu_tick(void);
bool modern_ui_menu_suspend(bool suspend);
bool modern_ui_confirm(const char* message);
void modern_mainmenu_activate(int button_index);
/* Strings returned by the engine adapters retain the game's encoding. */
typedef struct ModernCharacter {
    int64_t object;
    int portrait, index, count, description_kind;
    bool editable, gender_editable;
    char name[64];
    const char *gender, *race, *background, *description;
} ModernCharacter;
bool modern_mainmenu_character(ModernCharacter* character, int description);
void modern_mainmenu_gender(int direction);
void modern_mainmenu_name(const char* name);
const char* modern_mainmenu_save_name(int index);
void modern_mainmenu_save_select(int index);
struct GameSaveInfo;
const struct GameSaveInfo* modern_mainmenu_save_info(void);
const char* modern_mainmenu_save_location(void);
const char* modern_charedit_text(int number);
void modern_charedit_adjust(int category, int index, int direction);
void modern_ui_journal_open(void);
void modern_ui_journal_close(void);
void modern_ui_message(const char* text);

/* Read-only adapter over the original journal's localization and game data. */
int modern_logbook_select(int tab);
int modern_logbook_entry(int index, char* text, size_t capacity);
void modern_intgame_hud_hide(bool hide);
void modern_intgame_action(int action);
const char* modern_intgame_text(int number);
int modern_intgame_icon(int slot);
typedef struct ModernDetails {
    int count, icon, portrait;
    int bar_count,bar_values[2],bar_kinds[2],bar_lines[2];
    int extra_count,extra_icons[4];
    char title[2000];
    char lines[20][2000];
} ModernDetails;
void modern_inventory_details(int64_t pc,int64_t object,int kind,ModernDetails* details);
const ModernDetails* modern_hover_details(void);
void modern_map_pan(int dx,int dy);
int modern_fate_count(void);
const char* modern_fate_text(int index);
bool modern_fate_active(int index);
void modern_fate_choose(int index);
void modern_fate_sync(bool native);
typedef struct ModernBubble {
    const char* text;
    int type,x,y,width,height;
} ModernBubble;
int modern_bubbles(ModernBubble* bubbles,int capacity);
void modern_bubbles_sync(bool native);
typedef struct ModernOption {
    const char *label,*text;
    int value,maximum;
    bool enabled,numeric;
} ModernOption;
bool modern_cyclic_state(int id,ModernOption* option);
void modern_cyclic_adjust(int id,int direction);
bool modern_options_state(int index,ModernOption* option);
void modern_options_adjust(int index,int direction);
bool modern_options_select(int tab);
void modern_options_return(void);

/* Live panel adapters. Mouse coordinates are in the original 800x600 canvas;
 * the presentation maps its own layout to these engine-owned controls. */
struct DialogState;
const struct DialogState* modern_dialog_state(void);
void modern_dialog_choose(int index);
int64_t modern_dialog_actor(void);
bool modern_dialog_anchor(int* x,int* y);
int64_t modern_charedit_object(void);
bool modern_charedit_editable(void);
typedef struct ModernInventory {
    int mode, scroll, rows;
    int64_t pc, target, owner;
    bool paperdoll, target_paperdoll, drop, use, gamble;
    int drop_x, drop_y, use_x, use_y, gamble_x, gamble_y;
    const char* feedback;
} ModernInventory;
bool modern_inventory_state(ModernInventory* state);
int64_t modern_inventory_at(int x,int y);
enum { MODERN_INVENTORY_DOUBLE_CLICK = -1 };
void modern_inventory_mouse(int x,int y,int event);
void modern_inventory_action(int action);
void modern_inventory_scroll(int direction);
const char* modern_inventory_text(int number);
typedef struct ModernMap {
    int mode, x, y, width, height;
    void* buffer;
    bool moving;
} ModernMap;
bool modern_map_state(ModernMap* state);
void modern_map_mouse(int x,int y,int event);
void modern_map_action(int action);
const char* modern_map_text(int number);
void modern_quantity_state(int* value,int* maximum);
void modern_quantity_action(int value,bool accept);

void modern_sleep_sync(bool native);
int modern_sleep_count(void);
const char* modern_sleep_text(int option);
void modern_sleep_choose(int option);
typedef struct ModernSchematic {
    int tech,page,count,id,art,view,readiness;
    const char *name,*description;
    int64_t component1,component2;
} ModernSchematic;
bool modern_schematic_state(ModernSchematic* state);
void modern_schematic_action(int action);
const char* modern_schematic_text(int number);
#endif
