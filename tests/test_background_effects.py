"""Exercise native effect selection, preview restoration and bonus persistence."""
import os
import re
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
implementation = (root / 'source/src/game/modern_background_effects.h').read_text(encoding='utf-8')
implementation = re.sub(r'^#include .*$', '', implementation, flags=re.M)
effect = (root / 'source/src/game/effect.c').read_text(encoding='utf-8')
validation = effect.split('bool effect_is_single_stat_adjustment', 1)[1].split('/**', 1)[0]
validation = 'bool effect_is_single_stat_adjustment' + validation
source = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum {STAT_SPEED=11,STAT_MAX_FOLLOWERS=15};
enum {OBJ_F_CRITTER_STAT_BASE_IDX,OBJ_F_CRITTER_EFFECTS_IDX,OBJ_F_CRITTER_EFFECT_CAUSE_IDX,OBJ_F_TYPE};
enum {EFFECT_CAUSE_BACKGROUND=1,EFFECT_CAUSE_ITEM=5,EFFECT_OPERATOR_ADD};
#define EFFECT_LIST_CAPACITY 1000
typedef struct {int count,ids[7],params[7],operators[7];} Effect;
static Effect effect_stat_effects[1000],effect_basic_skill_effects[1000],effect_tech_skill_effects[1000];
static Effect effect_resistance_effects[1000],effect_tech_effects[1000],effect_special_effects[1000];
static bool effect_initialized=true;
static int raw[8],lengths[3],fields[3][256],callbacks;
static int obj_field_int32_get(int64_t o,int f){assert(o==1 && f==OBJ_F_TYPE);return 1;}
static bool obj_type_is_critter(int t){return t==1;}
static int obj_arrayfield_length_get(int64_t o,int f){assert(o==1);return lengths[f];}
/* Engine length_set deletes a key, rather than truncating to a length. Track
 * holes explicitly so an incomplete cleanup cannot pass as a dense array. */
static bool removed[3][256];
static void obj_arrayfield_length_set(int64_t o,int f,int n){
 assert(o==1 && n>=0 && n<256 && !removed[f][n]);removed[f][n]=true;lengths[f]--;
}
static unsigned obj_arrayfield_uint32_get(int64_t o,int f,int i){assert(o==1 && i<lengths[f]);return fields[f][i];}
static void obj_arrayfield_uint32_set(int64_t o,int f,int i,unsigned v){assert(o==1 && i<256);removed[f][i]=false;fields[f][i]=(int)v;if(i>=lengths[f])lengths[f]=i+1;}
static int obj_arrayfield_int32_get(int64_t o,int f,int i){assert(o==1 && f==0);return raw[i];}
static void obj_arrayfield_int32_set(int64_t o,int f,int i,int v){assert(o==1 && f==0);raw[i]=v;}
static void anim_speed_recalc(int64_t o){assert(o==1);callbacks++;}
static void effect_remove_internal(int64_t o,int i){
 assert(o==1);for(int f=1;f<=2;f++){for(int j=i;j+1<lengths[f];j++)fields[f][j]=fields[f][j+1];lengths[f]--;}
}
VALIDATION
static void effect_add(int64_t o,int id,int cause){
 obj_arrayfield_uint32_set(o,1,lengths[1],id);obj_arrayfield_uint32_set(o,2,lengths[2],cause);callbacks++;
}
static int stat_level_get(int64_t o,int stat){
 assert(o==1);int v=stat<8?raw[stat]:stat==STAT_SPEED?raw[1]:stat==STAT_MAX_FOLLOWERS?raw[7]/4:0;
 for(int i=0;i<lengths[1];i++){Effect* e=&effect_stat_effects[fields[1][i]];
  for(int j=0;j<e->count;j++)if(e->ids[j]==stat)v+=e->params[j];}
 return v;
}
IMPLEMENTATION
int main(void){
 for(int i=0;i<2;i++)for(int sign=0;sign<2;sign++){
  Effect* e=&effect_stat_effects[modern_background_bonus_effects[i][sign]];
  e->count=1;e->ids[0]=modern_background_bonus_stats[i];e->params[0]=sign?1:-1;e->operators[0]=EFFECT_OPERATOR_ADD;
 }
 assert(modern_background_bonuses_available());
 effect_special_effects[298].count=1;assert(!modern_background_bonuses_available());effect_special_effects[298].count=0;
 effect_stat_effects[297].params[0]=-2;assert(!modern_background_bonuses_available());effect_stat_effects[297].params[0]=-1;
 for(int i=0;i<8;i++)raw[i]=8;
 /* Same stock effect from an item must never be counted or removed as a background bonus. */
 effect_add(1,298,EFFECT_CAUSE_ITEM);effect_add(1,111,EFFECT_CAUSE_BACKGROUND);
 modern_background_bonus_set(1,0,2);modern_background_bonus_set(1,1,-1);
 assert(modern_background_bonus_get(1,0)==2 && modern_background_bonus_get(1,1)==-1);
 int saved_fields[3][256],saved_lengths[3],saved_raw[8];
 memcpy(saved_fields,fields,sizeof(fields));memcpy(saved_lengths,lengths,sizeof(lengths));memcpy(saved_raw,raw,sizeof(raw));
 int primary[8]={8,12,8,8,8,8,8,16},draft[2]={-1,2},values[16],before_callbacks=callbacks;
 modern_background_preview(1,primary,draft,values);
 assert(values[STAT_SPEED]==12 && values[STAT_MAX_FOLLOWERS]==6);
 assert(callbacks==before_callbacks && !memcmp(saved_lengths,lengths,sizeof(lengths)) && !memcmp(saved_raw,raw,sizeof(raw)));
 for(int f=1;f<=2;f++)assert(!memcmp(saved_fields[f],fields[f],lengths[f]*sizeof(int)));
 for(int frame=0;frame<100;frame++)modern_background_preview(1,primary,draft,values);
 assert(!memcmp(saved_lengths,lengths,sizeof(lengths)) && callbacks==before_callbacks);
 /* Applying previewed values yields the same effects, without duplicate stacks. */
 memcpy(raw,primary,sizeof(raw));modern_background_bonus_set(1,0,-1);modern_background_bonus_set(1,1,2);
 assert(stat_level_get(1,STAT_SPEED)==values[STAT_SPEED] && stat_level_get(1,STAT_MAX_FOLLOWERS)==values[STAT_MAX_FOLLOWERS]);
 int count=lengths[1];modern_background_bonus_set(1,1,2);assert(lengths[1]==count);
 modern_background_bonus_set(1,0,0);modern_background_bonus_set(1,1,0);
 assert(lengths[1]==2 && fields[1][0]==298 && fields[2][0]==EFFECT_CAUSE_ITEM && fields[1][1]==111);
 effect_stat_effects[298].params[0]=7;modern_background_bonus_set(1,0,2);assert(lengths[1]==2);
 puts("PASS: pure stock effects, cause isolation, exact preview rollback, apply parity, no duplicate stacks, incompatible rules guard");
}
'''.replace('VALIDATION', validation).replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-background-effects-') as tmp:
    code = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    code.write_text(source, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(code), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
