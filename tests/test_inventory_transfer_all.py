#!/usr/bin/env python3
"""Exercise bulk transfers through the native inventory transaction adapter."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_inventory_transfer.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum {INVEN_UI_MODE_LOOT,INVEN_UI_MODE_BARTER,INVEN_UI_MODE_STEAL};
enum {OBJ_F_FLAGS,OBJ_F_ITEM_FLAGS};
#define OF_OFF 1
#define OIF_NO_DISPLAY 2
#define ITEM_CANNOT_OK 0
#define IS_WEAR_INV_LOC(x) ((x)>=1000 && (x)<2000)
static bool inven_ui_created=true,network,ready=true,combat,close_after,mutate_next;
static int inven_ui_mode=INVEN_UI_MODE_LOOT,dword_6810FC,dword_681440=500;
static int64_t inven_ui_pc_obj=1,qword_6813A8=2,qword_682C78=2,inven_ui_drag_item_obj,qword_681450,qword_681458;
static int64_t leader=1,actor=1;
static int dword_68111C[1],dword_681518[1],calls,freed,refreshes;
static int64_t owners[20];
static int locations[20],flags[20],itemflags[20],blocked[20],no_room[20];
static bool tig_net_is_active(void){return network;}
static bool obj_handle_is_valid(int64_t obj){return obj>0 && obj<20 && (obj<3 || owners[obj]>0);}
static int64_t critter_leader_get(int64_t obj){assert(obj==2);return leader;}
static bool combat_turn_based_is_active(void){return combat;}
static bool combat_input_ready(int64_t obj){return ready && obj==actor;}
static int64_t combat_input_actor(void){return actor;}
static int item_list_get(int64_t owner,int64_t** out){*out=malloc(20*sizeof(**out));int n=0;for(int i=3;i<20;i++)if(owners[i]==owner)(*out)[n++]=i;return n;}
static void item_list_free(int64_t* p){free(p);freed++;}
static bool item_parent(int64_t item,int64_t* out){*out=owners[item];return *out>0;}
static int obj_field_int32_get(int64_t item,int field){return field==OBJ_F_FLAGS?flags[item]:itemflags[item];}
static int item_inventory_location_get(int64_t item){return locations[item];}
static int item_check_remove(int64_t item){return blocked[item];}
static int item_check_insert(int64_t item,int64_t to,int* loc){assert(to==1 || to==2);*loc=0;return no_room[item];}
static void sub_5788C0(int64_t item,int64_t to,int loc,int mode){assert(qword_681450==owners[item] && dword_681440==0 && loc==0 && mode==0x21);owners[item]=to;calls++;if(close_after)inven_ui_created=false;if(mutate_next)owners[item+1]=0;}
static void item_inventory_slots_get(int64_t owner,int* slots){assert(owner==1 || owner==2);*slots=1;}
static void redraw_inven(bool full){(void)full;refreshes++;}
''' + source + r'''
int main(void){
 owners[3]=owners[4]=owners[5]=owners[6]=owners[7]=owners[8]=owners[9]=2;
 locations[4]=1000;itemflags[5]=OIF_NO_DISPLAY;flags[6]=OF_OFF;blocked[7]=1;no_room[8]=1;
 modern_inventory_transfer_all(true);assert(calls==2 && owners[3]==1 && owners[9]==1 && owners[4]==2 && owners[7]==2 && owners[8]==2);
 modern_inventory_transfer_all(false);assert(calls==4 && owners[3]==2 && owners[9]==2);
 assert(freed==2 && refreshes==2);
 inven_ui_mode=INVEN_UI_MODE_STEAL;modern_inventory_transfer_all(true);assert(calls==4);
 inven_ui_mode=INVEN_UI_MODE_BARTER;modern_inventory_transfer_all(true);assert(calls==4);
 dword_6810FC=1;leader=0;modern_inventory_transfer_all(true);assert(calls==4);leader=1;
 inven_ui_drag_item_obj=3;modern_inventory_transfer_all(true);assert(calls==4);inven_ui_drag_item_obj=0;
 network=true;modern_inventory_transfer_all(true);assert(calls==4);network=false;
 actor=2;modern_inventory_transfer_all(true);assert(calls==4);actor=1;
 combat=true;ready=false;modern_inventory_transfer_all(true);assert(calls==4);ready=true;
 modern_inventory_transfer_all(true);assert(calls==6 && owners[3]==1);
 close_after=true;modern_inventory_transfer_all(false);assert(calls==7 && freed==4);
 inven_ui_created=true;close_after=false;mutate_next=true;owners[3]=owners[4]=2;locations[4]=0;
 modern_inventory_transfer_all(true);assert(calls==8 && owners[4]==0 && freed==5);
 puts("PASS: both directions, full-stack dispatch, worn/hidden/locked items, capacity, merchants, stealing, busy state and script mutation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-bulk-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
