"""Compile the actual starting-kit logic against a bounded native-inventory model."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
implementation = (root / 'source/src/ui/modern_background_loadout.inc').read_text(encoding='utf-8').split('static int background_item_rows')[0]
source = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define OBJ_F_LOCATION 1
#define OBJ_F_ITEM_FLAGS 2
#define OIF_IDENTIFIED 8
static int gold,items,ammo,capacity,created,destroyed,quantity,flags;
static int64_t prototype;
static void SDL_strlcpy(char* a,const char* b,size_t n){assert(strlen(b)<n);memcpy(a,b,strlen(b)+1);}
static int64_t sub_4685A0(int id){return id==6029 || id==7038 || id==8042 || id==48300?id+100000:0;}
static bool item_is_item(int64_t p){return p!=108042;}
static int sub_462410(int64_t p,int* q){*q=5;return p==107038?6:-1;}
static int64_t obj_field_handle_get(int64_t o,int f){(void)o;assert(f==6);return ammo?9999:0;}
static int obj_field_int32_get(int64_t o,int f){if(f==2)return flags;assert(o==9999 && f==5);return ammo;}
static int64_t obj_field_int64_get(int64_t o,int f){assert(o==1 || o==2);assert(f==1);return 20;}
static void obj_field_int32_set(int64_t o,int f,int v){assert(o==300);if(f==2)flags=v;else{assert(f==5);quantity=v;}}
static bool object_create(int64_t p,int64_t loc,int64_t* o){assert(loc==20);created++;prototype=p;*o=300;flags=quantity=0;return true;}
static bool item_transfer(int64_t o,int64_t pc){assert(o==300);assert(pc==1 || pc==2);assert(flags&OIF_IDENTIFIED);if(items>=capacity)return false;if(prototype==107038)ammo+=quantity;else items++;return true;}
static void object_destroy(int64_t o){assert(o==300);destroyed++;}
static int item_gold_get(int64_t o){(void)o;return gold;}
static bool item_gold_transfer(int64_t from,int64_t to,int n,int64_t obj){(void)obj;assert(n>=0);if(from)gold-=n;if(to)gold+=n;return true;}
IMPLEMENTATION
static bool visit_shop(int64_t obj){gold=400;items=ammo=0;return modern_background_loadout_generate(obj);}
int main(void){
 int n;assert(background_money_value("",&n)&&n==0);
 assert(background_money_value("2147483647",&n)&&n==INT_MAX);
 assert(!background_money_value("2147483648",&n));
 assert(!background_money_value("-1",&n));assert(!background_money_value("12x",&n));
 assert(background_item_allowed(6029));assert(!background_item_allowed(8042));
 assert(!background_item_allowed(48300));assert(!background_item_allowed(9056));
 capacity=4;modern_background_loadout_begin(0,1);background_loadout_open(1);
 strcpy(background_money,"12345");background_quantities[6029]=2;background_quantities[7038]=75;
 background_loadout_commit();assert(visit_shop(1));assert(items==2&&ammo==75&&gold==12345);
 assert(visit_shop(1));assert(items==2&&ammo==75&&gold==12345); /* Native reset on another shop visit. */
 background_loadout_open(1);background_quantities[6029]=3;strcpy(background_money,"999");
 background_loadout_open(1);assert(background_quantities[6029]==2&&!strcmp(background_money,"12345")); /* Cancel. */
 modern_background_loadout_begin(1,2);assert(visit_shop(2));assert(items==2&&gold==12345);
 assert(visit_shop(1));assert(items==0&&gold==400); /* Old object cannot receive the kit. */
 capacity=1;assert(!visit_shop(2));assert(background_loadout.failed&&destroyed==1);
 capacity=4;assert(visit_shop(2));assert(items==2&&!background_loadout.failed); /* Retry resets partial inventory. */
 background_loadout_open(2);strcpy(background_money,"0");background_loadout_commit();
 assert(visit_shop(2));assert(gold==0);
 background_loadout_open(2);strcpy(background_money,"2147483647");background_loadout_commit();
 assert(visit_shop(2));assert(gold==INT_MAX);
 background_loadout.quantities[6029]=0;background_loadout.quantities[7038]=INT_MAX;
 ammo=1;int before=created;assert(!modern_background_loadout_generate(2));assert(created==before&&ammo==1);
 background_loadout.quantities[7038]=0;background_loadout.quantities[4830]=1;
 assert(!visit_shop(2)); /* Unknown resource cannot be instantiated. */
 modern_background_loadout_begin(0,1);assert(visit_shop(1));assert(items==0&&gold==400);
 background_loadout_open(1);assert(!*background_money&&background_quantities[6029]==0);
 /* Keep editor state references exercised under -Werror. */
 assert(!background_equipment&&!background_item_page&&!background_item_search[0]);background_item_pages=1;assert(background_item_pages==1);
 puts("PASS: native kit, money bounds, cancel, Back, fresh character, repeated shop, capacity failure, retry, ammo overflow and resource guard");
}
'''.replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-loadout-') as tmp:
    code = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    code.write_text(source, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(code), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
