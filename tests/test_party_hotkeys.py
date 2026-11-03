#!/usr/bin/env python3
"""Exercise per-character shortcuts, stable identities and save compatibility."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root/'source/src/ui/modern_hotkeys.inc').read_text()
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int id;} ObjectID;
typedef struct {int64_t obj;struct {ObjectID objid;} field_8;} Ref;
typedef struct {int slot,type,data,count,flags,art_id;Ref item_obj;struct {int art_num;} info;} Hotkey;
enum {HOTKEY_NONE,HOTKEY_ITEM,HOTKEY_SKILL,HOTKEY_SPELL,HOTKEY_ITEM_SPELL};
#define TIG_ART_ID_INVALID -1
#define SPELL_COUNT 80
#define MT_SPELL_COUNT 256
typedef FILE TigFile;
#define tig_file_fwrite fwrite
#define tig_file_fread fread
#define tig_file_fclose fclose
static int64_t actor=1,item_owner=2;
static int generation,activated,activated_data;
static bool item_exists=true,known=true;
static Hotkey hero[10];
static int64_t combat_input_actor(void){return actor;}
static bool obj_handle_is_valid(int64_t obj){return obj>0;}
static bool player_is_local_pc_obj(int64_t obj){return obj==1;}
static ObjectID obj_get_id(int64_t obj){return (ObjectID){(int)obj-generation};}
static bool objid_is_equal(ObjectID a,ObjectID b){return a.id==b.id;}
static Hotkey* sub_57F240(int index){return &hero[index];}
static int64_t obj_pool_perm_lookup(ObjectID id){return item_exists?id.id+generation:0;}
static bool item_parent(int64_t item,int64_t* owner){*owner=item_owner;return item>0;}
static void sub_57E5A0(Hotkey* h){h->type=HOTKEY_NONE;h->item_obj.obj=0;h->art_id=-1;}
static void sub_4440E0(int64_t item,Ref* ref){ref->obj=item;ref->field_8.objid=obj_get_id(item);}
static int sub_554BE0(int64_t item){return (int)item;}
static int item_count_items_matching_prototype(int64_t owner,int64_t item){assert(owner==item_owner && item>0);return 3;}
static bool spell_is_known(int64_t who,int spell){assert(who>0 && spell>=0 && spell<80);return known;}
static bool combat_input_ready(int64_t who){return who>0;}
static bool item_is_identified(int64_t item){return item>0;}
static int item_magic_tech_complexity(int64_t item){return item>0?10:0;}
static bool magictech_is_tech(int spell){return spell==90;}
static bool sub_45A030(int spell){return spell==91;}
static int mt_item_spell(int64_t item,int slot){return item>0&&slot==0?42:-1;}
static void spell_ui_activate(int64_t item,int spell){assert(item==40);activated=40;activated_data=spell;}
static int spell_icon(int spell){return 100+spell;}
static int sub_579F70(int skill){return 200+skill;}
static int tig_art_interface_id_create(int id,int a,int b,int c,int* out){(void)a;(void)b;(void)c;*out=id;return 0;}
static void intgame_hotkey_activate(Hotkey* h){activated=(int)actor;activated_data=h->data;sub_57E5A0(h);}
static TigFile* tig_file_fopen(const char* name,const char* mode){(void)name;return fopen("hotkeys.dat",mode);}
''' + source + r'''
int main(void){
 hero[0]=(Hotkey){.type=HOTKEY_SKILL,.data=0};
 assert(modern_hotkey_get(0)==&hero[0]);
 actor=2;assert(!modern_hotkey_get(0));
 assert(companion_hotkey_assign(2,0,HOTKEY_SPELL,7,0));
 assert(companion_hotkey_assign(2,1,HOTKEY_ITEM,0,40));
 actor=3;assert(!modern_hotkey_get(0));
 assert(companion_hotkey_assign(3,0,HOTKEY_SKILL,2,0));
 actor=2;assert(modern_hotkey_get(0)->data==7 && hero[0].data==0);
 modern_hotkey_activate(0);assert(activated==2 && activated_data==7);
 assert(modern_hotkey_get(0)->type==HOTKEY_SPELL);
 assert(companion_hotkey_assign(2,10,HOTKEY_SKILL,1,0));
 actor=1;assert(companion_hotkey_assign(1,19,HOTKEY_SKILL,3,0));
 modern_hotkey_activate(19);assert(activated==1&&activated_data==3);
 assert(modern_hotkey_get(19)->type==HOTKEY_SKILL&&modern_hotkey_get(0)==&hero[0]);
 actor=2;
 assert(companion_hotkey_assign(2,12,HOTKEY_ITEM_SPELL,42,40));
 known=false;modern_hotkey_activate(12);assert(activated==40&&activated_data==42);known=true;
 assert(companion_hotkeys_save());companion_hotkey_count=0;
 assert(companion_hotkeys_load());assert(companion_hotkey_count==3);assert(modern_hotkey_get(12)->type==HOTKEY_ITEM_SPELL);
 assert(modern_hotkey_get(10)->data==1&&modern_hotkey_get(0)->data==7);
 actor=1;assert(modern_hotkey_get(19)->data==3);actor=2;
 /* Version-one files contain only ten slots per owner. */
 FILE* legacy=fopen("hotkeys.dat","wb");int version=1;
 fwrite(&version,sizeof(int),1,legacy);fwrite(&companion_hotkey_count,sizeof(int),1,legacy);
 for(int i=0;i<companion_hotkey_count;i++){
  CompanionHotkeys* bank=&companion_hotkeys[i];fwrite(&bank->owner,sizeof(ObjectID),1,legacy);
  for(int j=0;j<10;j++) {Hotkey* h=&bank->slots[j];fwrite(&h->type,sizeof(int),1,legacy);fwrite(&h->data,sizeof(int),1,legacy);fwrite(&h->item_obj.field_8.objid,sizeof(ObjectID),1,legacy);}
 }
 fclose(legacy);assert(companion_hotkeys_load());
 assert(modern_hotkey_get(0)->data==7&&modern_hotkey_get(10)->type==HOTKEY_NONE);
 generation=100;actor=102;item_owner=102;
 assert(modern_hotkey_get(0)->data==7);
 assert(modern_hotkey_get(1)->item_obj.obj==140);
 actor=103;assert(modern_hotkey_get(0)->data==2);
 actor=102;item_owner=103;assert(modern_hotkey_get(1)->type==HOTKEY_NONE);
 known=false;assert(modern_hotkey_get(0)->type==HOTKEY_NONE);known=true;
 assert(!modern_hotkey_get(-1) && !modern_hotkey_get(20));
 actor=0;assert(!modern_hotkey_get(0));
 remove("hotkeys.dat");assert(companion_hotkeys_load() && companion_hotkey_count==0);
 FILE* file=fopen("hotkeys.dat","wb");fputs("broken",file);fclose(file);
 assert(!companion_hotkeys_load() && companion_hotkey_count==0);
 puts("PASS: separate hero/companion banks, actor activation, persistent bindings, changed handles, removed items/spells and old saves");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-party-hotkeys-') as directory:
    temp = Path(directory)
    (temp/'test.c').write_text(harness)
    subprocess.run([os.environ.get('CC','cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(temp/'test.c'), '-o', str(temp/'test')], check=True)
    subprocess.run([str(temp/'test')], cwd=temp, check=True)
