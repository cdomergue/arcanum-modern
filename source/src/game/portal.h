#ifndef ARCANUM_GAME_PORTAL_H_
#define ARCANUM_GAME_PORTAL_H_

#include "game/context.h"

void portal_bust(int64_t portal_obj, int64_t triggerer_obj);
void portal_delete(int64_t portal_obj, bool a2);
bool portal_is_open(int64_t portal_obj);
void portal_toggle(int64_t portal_obj);
bool portal_open(int64_t portal_obj);
bool portal_close(int64_t portal_obj);

#endif /* ARCANUM_GAME_PORTAL_H_ */
