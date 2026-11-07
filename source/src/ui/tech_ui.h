#ifndef ARCANUM_GAME_TECH_UI_H_
#define ARCANUM_GAME_TECH_UI_H_

#include "game/context.h"

void tech_ui_inc_degree(int64_t obj, int tech);
void tech_ui_dec_degree(int64_t obj, int tech);
void tech_ui_adjust_degree(int64_t obj, int tech, int action);

#endif /* ARCANUM_GAME_TECH_UI_H_ */
