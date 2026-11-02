#!/usr/bin/env python3
"""Disband only the requested follower, including when party members are off-map."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
src = (root / 'source/src/game/obj.c').read_text()
start = src.index('bool obj_remove_follower_reference(')
body = src[start:src.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
enum { OID_TYPE_NULL, OID_TYPE_HANDLE, OID_TYPE_GUID, OBJ_F_CRITTER_FOLLOWER_IDX };
typedef struct {int type; union {int64_t h;} d;} ObjectID;
typedef struct {int count; ObjectID members[6];} Object;
static Object party;
static ObjectID obj_get_id(int64_t h){return (ObjectID){OID_TYPE_GUID,{h+100}};}
static Object* obj_lock(int64_t h){assert(h==1);return &party;}
static void obj_unlock(int64_t h){assert(h==1);}
static void sub_408D60(Object* o,int f,int* n){assert(f==OBJ_F_CRITTER_FOLLOWER_IDX);*n=o->count;}
static bool objid_is_equal(ObjectID a,ObjectID b){return a.type==b.type && a.d.h==b.d.h;}
static void obj_arrayfield_fetch(Object* o,int f,int i,ObjectID* e){(void)f;assert(i<o->count);*e=o->members[i];}
static void obj_arrayfield_store(Object* o,int f,int i,ObjectID* e){(void)f;assert(i<o->count);o->members[i]=*e;}
static void sub_408E70(Object* o,int f,int i){(void)f;assert(i==o->count-1);o->count--;}
''' + body + r'''
int main(void){
    ObjectID local={OID_TYPE_HANDLE,{10}}, absent={OID_TYPE_GUID,{4567}}, null={0}, last={OID_TYPE_HANDLE,{33}};
    party=(Object){6,{local,obj_get_id(20),absent,null,{OID_TYPE_HANDLE,{20}},last}};
    Object original=party;
    assert(!obj_remove_follower_reference(1,88));
    assert(memcmp(&party,&original,sizeof party)==0);
    assert(obj_remove_follower_reference(1,20));
    ObjectID expected[]={local,absent,null,last};
    assert(party.count==4);
    for(int i=0;i<4;i++)assert(objid_is_equal(party.members[i],expected[i]));
    assert(!obj_remove_follower_reference(1,20) && party.count==4);
    assert(obj_remove_follower_reference(1,33) && party.count==3);
    assert(objid_is_equal(party.members[1],absent));
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-follower-roster-') as directory:
    path = Path(directory)
    (path / 'roster.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path/'roster.c'), '-o', str(path/'roster')], check=True)
    subprocess.run([str(path/'roster')], check=True)
print('PASS: absent, repeated and duplicate followers; unaffected and unresolved identities preserved')
