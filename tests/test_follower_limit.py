#!/usr/bin/env python3
"""Exercise the production recruitment checks with the optional capacity rule."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]/'source/src/game'
def function(file,name):
 s=(root/file).read_text();m=re.search(r'^\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',s,re.M);assert m,name
 return s[m.start():s.index('\n}',m.end())+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define MODERN_UNLIMITED_FOLLOWERS_KEY "modern unlimited followers"
#define OBJ_F_SPELL_FLAGS 1
#define OSF_MIND_CONTROLLED 1
#define OBJ_F_NPC_FLAGS 2
#define ONF_FORCED_FOLLOWER 1
#define BASIC_SKILL_PERSUATION 1
#define TRAINING_MASTER 3
enum { STAT_CHARISMA,STAT_ALIGNMENT,STAT_LEVEL,STAT_MAX_FOLLOWERS };
enum { AI_FOLLOW_OK,AI_FOLLOW_ALREADY_IN_GROUP,AI_FOLLOW_DISLIKE,AI_FOLLOW_TOO_GOOD,AI_FOLLOW_TOO_BAD,AI_FOLLOW_LOW_LEVEL,AI_FOLLOW_NOT_ALLOWED,AI_FOLLOW_LIMIT_REACHED };
typedef struct { int field_14,field_18,field_1C,field_20; } AiParams;
static int settings,option,maximum=1,count=1,reaction=50,npc_alignment,pc_alignment,npc_level=1,pc_level=5;
static int training,flags;
static bool network;
static int settings_get_value(int* s,const char* key){(void)s;(void)key;return option;}
static bool tig_net_is_active(void){return network;}
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static int stat_level_get(int64_t obj,int stat){switch(stat){case STAT_MAX_FOLLOWERS:return maximum;case STAT_ALIGNMENT:return obj==1?pc_alignment:npc_alignment;case STAT_LEVEL:return obj==1?pc_level:npc_level;default:return 8;}}
static int critter_num_followers(int64_t obj,bool charisma){(void)obj;assert(charisma);return count;}
static int obj_field_int32_get(int64_t obj,int field){(void)obj;return field==OBJ_F_NPC_FLAGS?flags:0;}
static bool sub_459040(int64_t obj,int flag,int64_t* source){(void)obj;(void)flag;(void)source;return false;}
static int64_t critter_pc_leader_get(int64_t obj){(void)obj;return 0;}
static int64_t critter_leader_get(int64_t obj){(void)obj;return 0;}
static int basic_skill_training_get(int64_t obj,int skill){(void)obj;(void)skill;return training;}
static void ai_copy_params(int64_t obj,AiParams* p){(void)obj;*p=(AiParams){0,50,50,5};}
static int reaction_get(int64_t npc,int64_t pc){(void)npc;(void)pc;return reaction;}
'''+function('critter.c','critter_followers_unlimited')+function('critter.c','critter_follower_limit_reached')+function('ai.c','ai_check_follow')+r'''
int main(void){
 assert(!critter_followers_unlimited(1));
 assert(critter_follower_limit_reached(1));assert(ai_check_follow(2,1,false)==AI_FOLLOW_LIMIT_REACHED);
 maximum=0;assert(ai_check_follow(2,1,false)==AI_FOLLOW_NOT_ALLOWED);
 option=1;assert(!critter_follower_limit_reached(1));assert(ai_check_follow(2,1,false)==AI_FOLLOW_OK);
 maximum=7;count=100;assert(!critter_follower_limit_reached(1));assert(ai_check_follow(2,1,false)==AI_FOLLOW_OK);
 reaction=-1;assert(ai_check_follow(2,1,false)==AI_FOLLOW_DISLIKE);reaction=50;
 npc_level=20;assert(ai_check_follow(2,1,false)==AI_FOLLOW_LOW_LEVEL);npc_level=1;
 pc_alignment=100;assert(ai_check_follow(2,1,false)==AI_FOLLOW_TOO_GOOD);pc_alignment=-100;assert(ai_check_follow(2,1,false)==AI_FOLLOW_TOO_BAD);pc_alignment=0;
 assert(!critter_followers_unlimited(3));assert(critter_follower_limit_reached(3));
 network=true;assert(!critter_followers_unlimited(1));assert(ai_check_follow(2,1,false)==AI_FOLLOW_LIMIT_REACHED);network=false;
 option=0;assert(count==100);assert(critter_follower_limit_reached(1));assert(ai_check_follow(2,1,false)==AI_FOLLOW_LIMIT_REACHED);
 assert(ai_check_follow(2,1,true)==AI_FOLLOW_OK);
 training=TRAINING_MASTER;assert(ai_check_follow(2,1,false)==AI_FOLLOW_OK);training=0;
 flags=ONF_FORCED_FOLLOWER;assert(ai_check_follow(2,1,false)==AI_FOLLOW_OK);
 puts("PASS: default/re-enabled limits, unlimited party capacity, recruitment conditions, script capacity policy, forced/master exceptions and network isolation");
}
'''
# The script opcode must use the same capacity policy, not a second stat comparison.
script=(root/'script.c').read_text().split('case SCT_OBJ_HAS_MAX_FOLLOWERS: {')[1].split('} break;')[0]
assert 'critter_follower_limit_reached(objs[index])' in script
assert 'STAT_MAX_FOLLOWERS' not in script
with tempfile.TemporaryDirectory(prefix='arcanum-capacity-') as directory:
 p=Path(directory);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
