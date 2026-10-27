#!/usr/bin/env python3
"""Check production trap knowledge for followers and player-owned summons.

The real pathfinding and movement checks are in tools/check-companion-traps.c.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/trap.c').read_text()


def function(name):
    match = re.search(r'^bool ' + name + r'\([^\n]+\)\n\{', source, re.M)
    assert match, name
    return source[match.start():source.index('\n}', match.end()) + 2] + '\n'


prelude = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
enum { OBJ_HANDLE_NULL, OBJ_TYPE_PC, OBJ_TYPE_NPC, OBJ_TYPE_TRAP };
enum { OBJ_F_TYPE, OBJ_F_FLAGS, OBJ_F_SPELL_FLAGS };
enum { OF_TRAP_PC=1, OF_TRAP_SPOTTED=2, OSF_SUMMONED=4, OBJ_TM_TRAP=8 };
typedef struct ObjectNode {int64_t obj;struct ObjectNode* next;} ObjectNode;
typedef struct {ObjectNode* head;} ObjectList;
static int type[10]={0,OBJ_TYPE_PC,OBJ_TYPE_NPC,OBJ_TYPE_NPC,OBJ_TYPE_NPC,OBJ_TYPE_TRAP,OBJ_TYPE_TRAP};
static int flags[10],spells[10];
static int64_t leaders[10]={0,0,1},summoners[10];
static bool located=true;
static int listed,destroyed;
static ObjectNode known={6,NULL},unknown={5,&known};
static int obj_field_int32_get(int64_t obj,int field){assert(obj>0&&obj<10);return field==OBJ_F_TYPE?type[obj]:field==OBJ_F_FLAGS?flags[obj]:spells[obj];}
static bool obj_type_is_critter(int value){return value==OBJ_TYPE_PC||value==OBJ_TYPE_NPC;}
static bool player_is_pc_obj(int64_t obj){return type[obj]==OBJ_TYPE_PC;}
static int64_t critter_pc_leader_get(int64_t obj){return leaders[obj];}
static bool sub_459040(int64_t obj,int spell,int64_t* owner){assert(spell==OSF_SUMMONED);*owner=summoners[obj];return *owner!=0;}
static void object_list_location(int64_t loc,int mask,ObjectList* list){assert(loc==100&&mask==OBJ_TM_TRAP);listed++;list->head=located?&unknown:NULL;}
static void object_list_destroy(ObjectList* list){(void)list;destroyed++;}
'''
checks = r'''
int main(void){
 flags[6]=OF_TRAP_SPOTTED;
 assert(trap_party_avoids_location(2,100)); /* shared knowledge, second trap */
 assert(trap_party_avoids_location(1,100)); /* hero cannot resume through a known trap */
 assert(!trap_party_avoids_location(3,100)); /* unrelated NPC */
 assert(!trap_party_avoids_location(0,100));
 assert(!trap_party_avoids_location(5,100)); /* non-critter */
 flags[6]=0;assert(!trap_party_avoids_location(2,100));
 assert(!trap_party_avoids_location(1,100)); /* no knowledge of hidden traps */
 flags[6]=OF_TRAP_PC;assert(trap_party_avoids_location(2,100));
 spells[4]=OSF_SUMMONED;summoners[4]=1;
 assert(trap_party_avoids_location(4,100)); /* player summon */
 summoners[4]=2;assert(trap_party_avoids_location(4,100)); /* companion summon */
 flags[6]=0;assert(!trap_party_avoids_location(4,100)); /* no NPC omniscience */
 flags[6]=OF_TRAP_SPOTTED;summoners[4]=3;
 assert(!trap_party_avoids_location(4,100)); /* hostile summon */
 summoners[4]=0;assert(!trap_party_avoids_location(4,100));
 located=false;assert(!trap_party_avoids_location(2,100)); /* removed/disabled trap */
 assert(listed==destroyed);
 puts("PASS: hero and party trap avoidance, shared knowledge, summons, multiple traps, removal, NPC isolation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-companion-traps-') as directory:
    path = Path(directory)
    (path / 'check.c').write_text(prelude + function('trap_is_spotted')
                                + function('trap_party_avoids_location') + checks)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra',
                    '-Werror', str(path / 'check.c'), '-o', str(path / 'check')], check=True)
    subprocess.run([str(path / 'check')], check=True)
