#!/usr/bin/env python3
"""Check the production binding adapter's validation before native mutation."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/hotkey_ui.c').read_text()
a=s.index('bool modern_hotkey_assign(');body=s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#define OBJ_HANDLE_NULL 0
#define SPELL_COUNT 80
#define OBJ_F_TYPE 1
#define ITEM_CANNOT_OK 0
#define ITEM_CANNOT_NO_ROOM 1
#define TIG_MESSAGE_MOUSE_RIGHT_BUTTON_UP 5
enum { HOTKEY_NONE,HOTKEY_ITEM,HOTKEY_SKILL,HOTKEY_SPELL,HOTKEY_ITEM_SPELL };
enum { OBJ_TYPE_GOLD,OBJ_TYPE_AMMO,OBJ_TYPE_OTHER };
typedef struct {int64_t obj;} Ref;
typedef struct {int slot,type,data;Ref item_obj;} Hotkey;
static Hotkey stru_6835E0[10],stru_683950;
static int modern_hotkey_slot=-1,hotkey_ui_dragging_index=-1,calls,room=1,object_type=OBJ_TYPE_OTHER;
static bool hotkey_ui_dragging,ready=true,network,known=true;
static int64_t owner_id=1;
static int64_t combat_input_actor(void){return 1;}
static bool combat_input_ready(int64_t x){(void)x;return ready;}
static bool tig_net_is_active(void){return network;}
static bool player_is_local_pc_obj(int64_t actor){return actor==1;}
static bool companion_hotkey_assign(int64_t actor,int index,int type,int data,int64_t item){(void)actor;(void)index;(void)type;(void)data;(void)item;assert(0);return false;}
static bool spell_is_known(int64_t x,int n){(void)x;(void)n;return known;}
static bool obj_handle_is_valid(int64_t x){return x>0;}
static bool item_parent(int64_t x,int64_t* out){*out=owner_id;return x>0;}
static int obj_field_int32_get(int64_t x,int f){(void)x;(void)f;return object_type;}
static int item_check_remove(int64_t x){(void)x;return 0;}
static int item_find_free_inv_loc_for_insertion(int64_t x,int64_t p){(void)x;(void)p;return room?0:-1;}
static void item_error_msg(int64_t p,int reason){(void)p;assert(reason==ITEM_CANNOT_NO_ROOM);}
static void sub_575770(void){}
static void sub_573630(int64_t x){(void)x;}
static void sub_4440E0(int64_t x,Ref* r){r->obj=x;}
static bool sub_57E8D0(int event){assert(event==5);assert(modern_hotkey_slot>=0 && modern_hotkey_slot<10);calls++;stru_6835E0[modern_hotkey_slot]=stru_683950;return true;}
'''+body+r'''
int main(void){
 assert(!modern_hotkey_assign(-1,HOTKEY_SKILL,0,0));assert(!modern_hotkey_assign(10,HOTKEY_SKILL,0,0));
 assert(!modern_hotkey_assign(0,HOTKEY_SPELL,80,0));known=false;assert(!modern_hotkey_assign(0,HOTKEY_SPELL,2,0));known=true;
 ready=false;assert(!modern_hotkey_assign(0,HOTKEY_SKILL,0,0));ready=true;
 network=true;assert(!modern_hotkey_assign(0,HOTKEY_SKILL,0,0));network=false;
 owner_id=2;assert(!modern_hotkey_assign(0,HOTKEY_ITEM,0,42));owner_id=1;
 object_type=OBJ_TYPE_GOLD;assert(!modern_hotkey_assign(0,HOTKEY_ITEM,0,42));object_type=OBJ_TYPE_OTHER;
 assert(calls==0);
 assert(modern_hotkey_assign(0,HOTKEY_ITEM,0,42));assert(calls==1);assert(modern_hotkey_slot==-1);
 room=0;assert(!modern_hotkey_assign(0,HOTKEY_SPELL,2,0));assert(calls==1);assert(stru_6835E0[0].item_obj.obj==42);
 room=1;assert(modern_hotkey_assign(0,HOTKEY_SPELL,2,0));assert(stru_6835E0[0].item_obj.obj==0);
 assert(modern_hotkey_assign(9,HOTKEY_SKILL,3,0));assert(calls==3);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-hotkeys-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: slots, known spells, actor ownership, busy/network guards, full backpack and native dispatch')
