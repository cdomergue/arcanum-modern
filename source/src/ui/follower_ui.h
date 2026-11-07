#ifndef ARCANUM_UI_FOLLOWER_UI_H_
#define ARCANUM_UI_FOLLOWER_UI_H_

#include "game/context.h"
#include "game/target.h"

/**
 * Commands available in the follower drop-down context menu.
 *
 * Menu labels for each command is loaded from "mes\follower_ui.mes", keyed by
 * command index.
 */
typedef enum FollowerUiCommand {
    FOLLOWER_UI_COMMAND_WALK,
    FOLLOWER_UI_COMMAND_ATTACK,
    FOLLOWER_UI_COMMAND_STAY_CLOSE,
    FOLLOWER_UI_COMMAND_SPREAD_OUT,
    FOLLOWER_UI_COMMAND_BACK_OFF,
    FOLLOWER_UI_COMMAND_WAIT,
    FOLLOWER_UI_COMMAND_INVENTORY,
    FOLLOWER_UI_COMMAND_CHARACTER_SHEET,
    FOLLOWER_UI_COMMAND_COUNT,
} FollowerUiCommand;

bool follower_ui_init(GameInitInfo* init_info);
void follower_ui_exit(void);
void follower_ui_reset(void);
void follower_ui_resize(GameResizeInfo* resize_info);
bool follower_ui_load(GameLoadInfo* load_info);
bool follower_ui_save(TigFile* stream);
void follower_ui_execute_order(TargetDescriptor* td);
void follower_ui_end_order_mode(void);
void follower_ui_update(void);
void follower_ui_update_obj(int64_t obj);
void follower_ui_refresh(void);
void follower_ui_hide(void);
void follower_ui_show(void);

bool modern_party_detached(int64_t obj);
bool modern_party_set_grouped(int64_t obj, bool grouped);
int64_t modern_party_selected(void);
bool modern_party_select(int64_t obj);
void modern_followers_sync(bool hidden);
int modern_followers_get(int64_t* objects, int capacity);
const char* modern_follower_text(int command);
void modern_follower_log_menu(int64_t obj);
bool modern_follower_command_available(int64_t obj, int command);
void modern_follower_command(int64_t obj, int command);

#endif /* ARCANUM_UI_FOLLOWER_UI_H_ */
