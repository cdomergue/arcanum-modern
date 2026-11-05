"""Exercise native rule parsing, immutable resources and live effect calculations."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
def enum(file, name):
    return re.search(r'typedef enum '+name+r' \{.*?\} '+name+r';', (root/'source/src/game'/file).read_text(), re.S).group()
engine=(root/'source/src/game/effect.c').read_text(encoding='utf-8')
metadata=engine[engine.index('#define EFFECT_LIST_CAPACITY'):engine.index('#include "modern_background_rules.inc"')]
metadata=re.sub(r'^static (?:void|int) effect_.*?;\n', '', metadata, flags=re.M)
rules=(root/'source/src/game/modern_background_rules.inc').read_text(encoding='utf-8')
rules=re.sub(r'^#include "game/.*?"\n', '', rules, flags=re.M)
header=(root/'source/src/game/modern_background_rules.h').read_text()
math=engine[engine.index('static int effect_bounded'):engine.index('\n/**',engine.index('static int effect_bounded'))]
source=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdarg.h>
ENUMS
HEADER
METADATA
enum {OBJ_F_TYPE,OBJ_F_CRITTER_EFFECTS_IDX,OBJ_F_CRITTER_EFFECT_CAUSE_IDX};
static unsigned fields[3][512];static int lengths[3];static bool rename_ok=true;
static int obj_arrayfield_length_get(int64_t o,int f){assert(o==1);return lengths[f];}
static unsigned obj_arrayfield_uint32_get(int64_t o,int f,int i){assert(o==1&&i<lengths[f]);return fields[f][i];}
static void obj_arrayfield_uint32_set(int64_t o,int f,int i,unsigned v){assert(o==1&&i<512);fields[f][i]=v;if(lengths[f]<=i)lengths[f]=i+1;}
static void obj_arrayfield_length_set(int64_t o,int f,int n){assert(o==1);lengths[f]=n;}
static int obj_field_int32_get(int64_t o,int f){assert(o==1&&f==OBJ_F_TYPE);return 1;}
static bool obj_type_is_critter(int t){return t==1;}
static bool obj_pool_walk_first(int64_t* o,int* i){*o=1;*i=0;return true;}
static bool obj_pool_walk_next(int64_t* o,int* i){(void)o;(void)i;return false;}
static void tig_debug_printf(const char* f,...){(void)f;}
static bool SDL_RenamePath(const char* a,const char* b){if(!rename_ok)return false;return rename(a,b)==0;}
static void effect_add(int64_t o,int id,int c){int n=lengths[1];obj_arrayfield_uint32_set(o,1,n,id);obj_arrayfield_uint32_set(o,2,n,c);}
static void effect_remove_one_with_cause(int64_t o,int id,int c){assert(o==1);for(int i=0;i<lengths[1];i++)if(fields[1][i]==(unsigned)id&&fields[2][i]==(unsigned)c){for(int j=i+1;j<lengths[1];j++){fields[1][j-1]=fields[1][j];fields[2][j-1]=fields[2][j];}lengths[1]--;lengths[2]--;break;}}
RULES
MATH
static void install_native(int id,const char* t){BackgroundRulePlan p;assert(background_rule_parse(t,&p,NULL,0));assert(p.count==1);background_rule_install(id,p.groups[0]);background_effect_used[id]=true;}
static void reset(void){memset(fields,0,sizeof(fields));memset(lengths,0,sizeof(lengths));}
int main(void){
 for(int g=0;g<6;g++){
  Effect* p=calloc(EFFECT_LIST_CAPACITY,sizeof(*p));assert(p);
  switch(g){case 0:effect_stat_effects=p;break;case 1:effect_basic_skill_effects=p;break;case 2:effect_tech_skill_effects=p;break;case 3:effect_resistance_effects=p;break;case 4:effect_tech_effects=p;break;default:effect_special_effects=p;}
 }
 effect_initialized=true;background_rules_store_clear();assert(background_rules_store_load());
 install_native(341,"magicpts +4, reaction -20, alignment -200, alignment max -200");
 background_effect_used[399]=true; /* An installed empty ID is still reserved. */
 char error[256];BackgroundRules r={.inherit=true};
 strcpy(r.text,"reaction +5, alignment max -100, melee +4, resistfire +10%, expertiseelectric +3");
 assert(background_rules_prepare(&r,error,sizeof(error))&&r.count==1&&r.ids[0]==398);
 int first=r.ids[0];assert(background_rules_available(&r));
 effect_add(1,341,EFFECT_CAUSE_BACKGROUND);effect_add(1,341,EFFECT_CAUSE_ITEM);
 unsigned snapshot[3][512];memcpy(snapshot,fields,sizeof(fields));
 assert(background_rules_preview_begin(1,341,NULL,r.text,false));
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,50,effect_special_effects,false)==35); /* unrelated item stays */
 assert(effect_adjust_func(1,STAT_ALIGNMENT,0,effect_stat_effects,false)==-200);
 assert(!background_rules_preview_begin(1,341,NULL,r.text,false));
 background_rules_preview_end(1);assert(lengths[1]==2&&lengths[2]==2);
 assert(!memcmp(fields[1],snapshot[1],2*sizeof(unsigned)));
 assert(!memcmp(fields[2],snapshot[2],2*sizeof(unsigned)));
 assert(fields[1][0]==341&&fields[1][1]==341&&fields[2][1]==EFFECT_CAUSE_ITEM);
 reset();effect_add(1,341,EFFECT_CAUSE_BACKGROUND);r.inherit=false;background_rules_apply(1,341,&r);
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,50,effect_special_effects,false)==55);
 assert(effect_adjust_func(1,STAT_ALIGNMENT,0,effect_stat_effects,false)==-100);
 assert(effect_adjust_func(1,STAT_ALIGNMENT,-500,effect_stat_effects,false)==-500);
 assert(effect_adjust_func(1,0,10,effect_resistance_effects,false)==10);
 assert(effect_adjust_func(1,1,10,effect_resistance_effects,false)==11);
 assert(background_rules_saved_objects_valid());
 BackgroundRules revised=r;strcpy(revised.text,"reaction -7");assert(background_rules_prepare(&revised,error,sizeof(error)));
 assert(revised.ids[0]!=first&&effect_adjust_func(1,EFFECT_SPECIAL_REACTION,50,effect_special_effects,false)==55);
 BackgroundRules same=revised;assert(background_rules_prepare(&same,error,sizeof(error))&&same.ids[0]==revised.ids[0]);
 background_rules_remove(1,&r);background_rules_apply(1,341,&revised);assert(lengths[1]==1);
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,50,effect_special_effects,false)==43);
 /* Simulate restart with original resources plus immutable added records. */
 background_rules_store_clear();for(int g=0;g<6;g++)memset(background_rule_table(g),0,EFFECT_LIST_CAPACITY*sizeof(Effect));
 install_native(341,"magicpts +4, reaction -20, alignment -200, alignment max -200");background_effect_used[399]=true;
 assert(background_rules_store_load()&&background_rules_available(&r)&&background_rules_available(&revised));
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,50,effect_special_effects,false)==43);
 assert(background_rules_saved_objects_valid());
 background_effect_used[revised.ids[0]]=false;assert(!background_rules_saved_objects_valid());background_effect_used[revised.ids[0]]=true;
 /* Legacy speed/follower IDs must not become indistinguishable from old controls. */
 install_native(297,"speed +1");
 BackgroundRules speed={.inherit=true};strcpy(speed.text,"speed +1");
 assert(background_rules_prepare(&speed,error,sizeof(error))&&speed.ids[0]!=297);
 assert(background_rules_validate("ALIGNMENT MAX -200, reaction MIN -1",error,sizeof(error)));
 /* Invalid expressions leave the existing resource and catalogue untouched. */
 const char* invalid[]={"alignment max nope","reaction /0","melee +1%oops","race +1","reaction *11","unknown +1","reaction +10001"};
 for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++)assert(!background_rules_validate(invalid[i],error,sizeof(error))&&*error);
 BackgroundRules fail={.inherit=true};strcpy(fail.text,"reaction +39");rename_ok=false;
 assert(!background_rules_prepare(&fail,error,sizeof(error))&&fail.count==0&&background_rules_available(&r));rename_ok=true;
 /* Pack beyond the original five-changes-per-type limit without dropping terms. */
 BackgroundRules packed={.inherit=true};strcpy(packed.text,"st +1, dx +2, cn +3, be +4, in +5, pe +6, wp +7, ch +8");
 assert(background_rules_prepare(&packed,error,sizeof(error))&&packed.count==2);
 reset();background_rules_apply(1,341,&packed);assert(effect_adjust_func(1,STAT_CHARISMA,8,effect_stat_effects,false)==16);
 /* All operators use native order; -1 is a valid min/max. */
 assert(background_rules_preview_begin(1,341,&packed,"reaction +10%, reaction *2, reaction /4, reaction -5, reaction max 100",false));
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,100,effect_special_effects,false)==50);background_rules_preview_end(1);
 assert(background_rules_preview_begin(1,341,&packed,"alignment max -1",false));
 assert(effect_adjust_func(1,STAT_ALIGNMENT,20,effect_stat_effects,false)==-1);background_rules_preview_end(1);
 assert(background_rules_preview_begin(1,341,&packed,"alignment min -1",false));
 assert(effect_adjust_func(1,STAT_ALIGNMENT,-20,effect_stat_effects,false)==-1);background_rules_preview_end(1);
 assert(background_rules_preview_begin(1,341,&packed,"reaction *10, reaction *10, reaction *10, reaction *10, reaction *10, reaction *10, reaction *10, reaction *10, reaction *10, reaction *10",false));
 assert(effect_adjust_func(1,EFFECT_SPECIAL_REACTION,INT_MAX,effect_special_effects,false)==INT_MAX);background_rules_preview_end(1);
 char text[2048]="reaction -20, alignment max -200, reaction +2";
 assert(background_rules_step(text,sizeof(text),"reaction",1));assert(background_rules_adjustment(text,"reaction")==-17&&strstr(text,"alignment max -200"));
 assert(background_rules_describe(341,text,sizeof(text))&&strstr(text,"alignment max -200"));
 /* Conflicting resource IDs fail closed; IDs belonging to old saves are never reused. */
 background_rules_store_clear();install_native(first,"reaction -99");assert(!background_rules_store_load());
 background_rules_store_clear();for(int id=50;id<=399;id++)background_effect_used[id]=true;
 BackgroundRules exhausted={0};strcpy(exhausted.text,"reaction +723");assert(!background_rules_prepare(&exhausted,error,sizeof(error)));
 background_rules_store_clear();for(int g=0;g<6;g++)free(background_rule_table(g));
 puts("PASS: native syntax, operator order, negative caps, safe arithmetic, packing, exact preview, inheritance, immutable save IDs, reload, conflicts, atomic write failure and exhaustion");
}
'''
enums='\n'.join(enum(*pair) for pair in [('stat.h','Stat'),('skill.h','BasicSkill'),('skill.h','TechSkill'),('resistance.h','ResistanceType'),('tech.h','Tech'),('effect.h','EffectCause')])
source=source.replace('ENUMS',enums).replace('\nHEADER\n','\n'+header+'\n').replace('\nMETADATA\n','\n'+metadata+'\n').replace('\nRULES\n','\n'+rules+'\n').replace('\nMATH\n','\n'+math+'\n')
with tempfile.TemporaryDirectory(prefix='renaissance-native-rules-') as tmp:
    code=Path(tmp)/'test.c';binary=Path(tmp)/'test';code.write_text(source,encoding='utf-8');(Path(tmp)/'modern-ui').mkdir()
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(code),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True)
