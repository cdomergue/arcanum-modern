#!/usr/bin/env python3
"""Saving/loading with a follower on another map must retain its identity."""
from pathlib import Path
import os,re,subprocess,tempfile
source=(Path(__file__).resolve().parents[1]/'source/src/game/obj.c').read_text()
def function(name):
    match=re.search(r'^(?:static )?\w[^\n]*\b'+name+r'\([^;\n]*\)\n\{',source,re.M)
    assert match,name
    return source[match.start():source.index('\n}',match.end())+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_CRITTER_FOLLOWER_IDX 1
#define OBJ_F_FLAGS 2
#define OID_TYPE_NULL 0
#define OID_TYPE_PERMANENT 2
#define OID_TYPE_HANDLE 3
#define OF_DESTROYED 1
#define OF_EXTINCT 2
typedef struct {int type;union{int64_t h,id;}d;} ObjectID;
typedef struct {int type;} Object;
static Object object;
static ObjectID reference;
static bool present;
static int flags;
static Object* obj_lock(int64_t obj){assert(obj==1);return &object;}
static void obj_unlock(int64_t obj){assert(obj==1);}
static bool object_field_valid(int type,int fld){(void)type;return fld==1||fld==2;}
static void object_field_not_exists(Object* obj,int fld){(void)obj;(void)fld;assert(false);}
static void obj_arrayfield_fetch(Object* obj,int fld,int index,ObjectID* out){assert(obj==&object&&index==0&&(fld==1||fld==2));*out=reference;}
static int64_t obj_pool_perm_lookup(ObjectID oid){assert(oid.type==OID_TYPE_PERMANENT&&oid.d.id==1234);return present?7:0;}
static bool obj_handle_is_valid(int64_t obj){return obj==0||obj==7;}
static int obj_field_int32_get(int64_t obj,int fld){assert(obj==7&&fld==OBJ_F_FLAGS);return flags;}
static ObjectID obj_get_id(int64_t obj){assert(obj==7);return (ObjectID){OID_TYPE_PERMANENT,{.id=1234}};}
static ObjectID obj_pool_perm_reverse_lookup(int64_t obj){assert(obj==99);return obj_get_id(7);}
'''
for name in ['convert_follower_id_to_handle','convert_handle_to_id','convert_id_to_handle','obj_arrayfield_handle_get']:
    code+=function(name)
code+=r'''
int main(void){
 ObjectID saved={OID_TYPE_PERMANENT,{.id=1234}};reference=saved;
 assert(convert_follower_id_to_handle(&reference,0)&&!memcmp(&reference,&saved,sizeof(saved)));
 assert(!obj_arrayfield_handle_get(1,1,0));
 /* Autosave again before the player joins the separated follower. */
 assert(convert_handle_to_id(&reference,0)&&!memcmp(&reference,&saved,sizeof(saved)));
 present=true;assert(obj_arrayfield_handle_get(1,1,0)==7);
 assert(!obj_arrayfield_handle_get(1,2,0)); /* Other field behavior is unchanged. */
 assert(convert_follower_id_to_handle(&reference,0)&&reference.type==OID_TYPE_HANDLE&&reference.d.h==7);
 assert(convert_handle_to_id(&reference,0)&&!memcmp(&reference,&saved,sizeof(saved)));
 present=false;assert(convert_id_to_handle(&reference,0)&&reference.type==OID_TYPE_NULL);
 reference=(ObjectID){OID_TYPE_HANDLE,{.h=7}};flags=OF_DESTROYED|OF_EXTINCT;
 assert(convert_handle_to_id(&reference,0)&&reference.type==OID_TYPE_PERMANENT);
 reference=(ObjectID){OID_TYPE_HANDLE,{.h=7}};flags=OF_DESTROYED;
 assert(convert_handle_to_id(&reference,0)&&reference.type==OID_TYPE_NULL);
 puts("PASS: cross-map follower IDs survive load/save, resolve on arrival, and preserve normal field/deletion behavior");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-follower-refs-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
