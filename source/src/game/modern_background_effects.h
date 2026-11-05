#ifndef MODERN_BACKGROUND_EFFECTS_H
#define MODERN_BACKGROUND_EFFECTS_H

#include "game/effect.h"
#include "game/stat.h"

/* Stock effects: speed -1/+1 and follower limit -1/+1. These IDs and their
 * native Background cause are understood by the classic executable. */
static const int modern_background_bonus_stats[2]={STAT_SPEED,STAT_MAX_FOLLOWERS};
static const int modern_background_bonus_effects[2][2]={{297,298},{307,308}};

static inline bool modern_background_bonuses_available(void)
{
    for(int i=0;i<2;i++)for(int sign=0;sign<2;sign++)
        if(!effect_is_single_stat_adjustment(modern_background_bonus_effects[i][sign],
            modern_background_bonus_stats[i],sign?1:-1))return false;
    return true;
}

static inline int modern_background_bonus_get(int64_t obj,int which)
{
    int bonus=0,count=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);
    for(int i=0;i<count;i++)if(obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i)==EFFECT_CAUSE_BACKGROUND) {
        unsigned effect=obj_arrayfield_uint32_get(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);
        if(effect==(unsigned)modern_background_bonus_effects[which][0])bonus--;
        if(effect==(unsigned)modern_background_bonus_effects[which][1])bonus++;
    }
    return bonus;
}

static inline void modern_background_bonus_set(int64_t obj,int which,int bonus)
{
    if(!modern_background_bonuses_available() || modern_background_bonus_get(obj,which)==bonus)return;
    for(int sign=0;sign<2;sign++)while(effect_remove_one_with_cause(obj,
        modern_background_bonus_effects[which][sign],EFFECT_CAUSE_BACKGROUND)){}
    int effect=modern_background_bonus_effects[which][bonus>0];
    int count=bonus<0?-bonus:bonus;
    for(int i=0;i<count;i++)effect_add(obj,effect,EFFECT_CAUSE_BACKGROUND);
}

/* Read-only from the caller's perspective: use the engine's formulas with a
 * scoped draft, then restore raw fields and array lengths before returning.
 * No effect_add/stat setter, animation callback, frame or save occurs here. */
static inline void modern_background_preview(int64_t obj,const int primary[8],
    const int bonuses[2],int values[16])
{
    int original[8];
    for(int i=0;i<8;i++) {
        original[i]=obj_arrayfield_int32_get(obj,OBJ_F_CRITTER_STAT_BASE_IDX,i);
        obj_arrayfield_int32_set(obj,OBJ_F_CRITTER_STAT_BASE_IDX,i,primary[i]);
    }
    int effects=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);
    int causes=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX);
    if(modern_background_bonuses_available())for(int i=0;i<2;i++) {
        int delta=bonuses[i]-modern_background_bonus_get(obj,i);
        int count=delta<0?-delta:delta;
        for(int n=0;n<count;n++) {
            int at=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX);
            obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,at,modern_background_bonus_effects[i][delta>0]);
            obj_arrayfield_uint32_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,at,EFFECT_CAUSE_BACKGROUND);
        }
    }
    for(int i=0;i<16;i++)values[i]=stat_level_get(obj,i);
    /* Despite its name, length_set removes one sparse-array key; it does not
     * resize the collection. Remove every appended key, highest first. */
    for(int i=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECTS_IDX)-1;i>=effects;i--)
        obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECTS_IDX,i);
    for(int i=obj_arrayfield_length_get(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX)-1;i>=causes;i--)
        obj_arrayfield_length_set(obj,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,i);
    for(int i=0;i<8;i++)obj_arrayfield_int32_set(obj,OBJ_F_CRITTER_STAT_BASE_IDX,i,original[i]);
}

#endif
