#ifndef ARCANUM_GAME_MODERN_HOUSE_H_
#define ARCANUM_GAME_MODERN_HOUSE_H_
#include "game/script.h"
bool modern_house_location(int index, int* x, int* y, const char** name);
void modern_house_load(void);
void modern_house_tick(void);
const char* modern_house_description(int num);
bool modern_house_script(ScriptInvocation* invocation, bool* result);
#endif
