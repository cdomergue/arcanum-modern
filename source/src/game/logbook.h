#ifndef ARCANUM_GAME_LOGBOOK_H_
#define ARCANUM_GAME_LOGBOOK_H_

#include "game/context.h"

typedef enum LogbookKill {
    LBK_TOTAL_KILLS,
    LBK_MOST_POWERFUL_NAME,
    LBK_MOST_POWERFUL_LEVEL,
    LBK_LEAST_POWERFUL_NAME,
    LBK_LEAST_POWERFUL_LEVEL,
    LBK_MOST_GOOD_NAME,
    LBK_MOST_GOOD_VALUE,
    LBK_MOST_EVIL_NAME,
    LBK_MOST_EVIL_VALUE,
    LBK_MOST_MAGICAL_NAME,
    LBK_MOST_MAGICAL_VALUE,
    LBK_MOST_TECH_NAME,
    LBK_MOST_TECH_VALUE,
    LBK_COUNT,
} LogbookKill;

typedef enum LogbookInjury {
    LBI_BLINDED,
    LBI_CRIPPLED_ARM,
    LBI_CRIPPLED_LEG,
    LBI_SCARRED,
} LogbookInjury;

void logbook_add_kill(int64_t pc_obj, int64_t npc_obj);
void logbook_get_kills(int64_t pc_obj, int* values);
void logbook_add_injury(int64_t pc_obj, int64_t inflicted_by_obj, int injury);
int logbook_find_first_injury(int64_t obj, int* desc_ptr, int* injury_ptr);
int logbook_find_next_injury(int64_t obj, int idx, int* desc_ptr, int* injury_ptr);

#endif /* ARCANUM_GAME_LOGBOOK_H_ */
