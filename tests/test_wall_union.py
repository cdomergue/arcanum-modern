#!/usr/bin/env python3
"""Check production wall union, corner precedence and sector/roof lifetimes."""
from pathlib import Path
import os,re,subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/game/wallcheck.c').read_text()
def function(name):
    return re.search(r'^(?:static )?(?:void|bool) '+name+r'\([^\n]*\)\n\{.*?^\}',s,re.M|re.S).group()
names=['wallcheck_begin','sub_4386B0','sub_4387C0','sub_438720','sub_438570','wallcheck_roof_faded_clear','sub_438830']
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define REALLOC realloc
#define OBJ_F_LOCATION 1
#define OBJ_F_WALL_FLAGS 2
#define OBJ_F_RENDER_FLAGS 3
#define ORF_01000000 0x1000000
#define tig_debug_printf(...) assert(0)
typedef int Sector;
typedef int TigRect;
'''+''.join(re.search(r'typedef struct '+n+r' \{.*?\} '+n+';',s,re.S).group()+'\n' for n in ['S5E0A10','S5E0E20'])+r'''
static S5E0A10* stru_5E0A10;
static S5E0E20* stru_5E0E20;
static int dword_5E2E24,dword_5E2E28,wallcheck_wall_capacity,wallcheck_roof_capacity,wallcheck_collection_index;
static int locks,roof_count,invalidations,wall_flags[1000],render_flags[1000],roof_fades[1000];
static bool sector_lock(int64_t id,Sector** out){static Sector sector;(void)id;*out=&sector;locks++;return true;}
static void sector_unlock(int64_t id){(void)id;assert(locks>0);locks--;}
static int64_t obj_field_int64_get(int64_t obj,int field){assert(field==OBJ_F_LOCATION);return (obj/2)*4;}
static int64_t roof_normalize_loc(int64_t loc){return loc;}
static void roof_fade_on(int64_t loc){assert(!roof_fades[loc/4]);roof_fades[loc/4]=1;roof_count++;}
static void roof_fade_off(int64_t loc){assert(roof_fades[loc/4]);roof_fades[loc/4]=0;roof_count--;}
static int obj_field_int32_get(int64_t obj,int field){return field==OBJ_F_WALL_FLAGS?wall_flags[obj]:render_flags[obj];}
static void obj_field_int32_set(int64_t obj,int field,int value){if(field==OBJ_F_WALL_FLAGS)wall_flags[obj]=value;else render_flags[obj]=value;}
static void object_get_rect(int64_t obj,int flags,TigRect* rect){(void)flags;*rect=obj;}
static void invalidate(TigRect* rect){assert(*rect>0);invalidations++;}
static void (*dword_5E0A00)(TigRect*)=invalidate;
'''+ '\n'.join(function(n) for n in names)+r'''
int main(void){
 wallcheck_begin();wallcheck_collection_index++;
 for(int i=1;i<=600;i++)sub_438570(i,i/30,1);
 sub_438570(1,0,2); /* Native last-write precedence within one observer. */
 assert(stru_5E0E20[0].flags==(0x08|0x02));
 wallcheck_collection_index++;sub_438570(1,0,1); /* Another observer needs the other side. */
 assert(stru_5E0E20[0].flags==(0x04|0x08|0x02));
 sub_438830();assert(dword_5E2E24==600 && locks==600 && roof_count==301);
 assert(wallcheck_wall_capacity>=600 && wallcheck_roof_capacity>=301);
 assert(wall_flags[1]==6);
 for(int i=2;i<=600;i++)assert(wall_flags[i]==2);
 wallcheck_begin();wallcheck_collection_index++;sub_438570(2,0,2);sub_438830();
 assert(dword_5E2E24==1 && locks==1 && dword_5E2E28==1 && roof_count==1);
 for(int i=1;i<=600;i++)assert(wall_flags[i]==(i==2?4:0));
 wallcheck_begin();sub_438830();assert(!locks&&!roof_count&&!dword_5E2E24&&!dword_5E2E28);
 free(stru_5E0E20);free(stru_5E0A10);
 puts("PASS: 600-wall union, 301 roof references, native corner precedence, shared sides and complete cleanup");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-wall-union-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
