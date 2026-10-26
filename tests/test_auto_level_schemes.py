#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/charedit_ui.c').read_text()
start = source.index('int modern_charedit_scheme_count(void)')
end = source.index('\n}', source.index('void modern_charedit_scheme_select(', start)) + 2
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define OBJ_HANDLE_NULL 0
#define CHAREDIT_MODE_PASSIVE 2
static bool charedit_created=true;
static int charedit_mode=2,charedit_num_schemes=4,charedit_scheme_ids[]={0,1,50,199};
static int64_t charedit_obj=2,pc=1,leader=1;
static int selected[4]={0,50,50,50},refreshes;
static int64_t player_get_local_pc_obj(void){return pc;}
static int64_t critter_pc_leader_get(int64_t obj){return obj==2?leader:0;}
static void auto_level_scheme_set(int64_t obj,int scheme){assert(obj>0&&obj<4);selected[obj]=scheme;}
static void charedit_refresh(void){refreshes++;}
''' + source[start:end] + r'''
int main(void){
 assert(modern_charedit_scheme_count()==4);
 assert(modern_charedit_scheme_id(2)==50&&modern_charedit_scheme_id(3)==199);
 assert(modern_charedit_scheme_id(-1)==-1&&modern_charedit_scheme_id(4)==-1);
 assert(modern_charedit_scheme_editable());modern_charedit_scheme_select(3);
 assert(selected[2]==199&&selected[1]==50&&refreshes==1);
 modern_charedit_scheme_select(-1);modern_charedit_scheme_select(4);assert(refreshes==1);
 modern_charedit_scheme_select(0);assert(selected[2]==0&&refreshes==2);
 leader=0;assert(!modern_charedit_scheme_editable());modern_charedit_scheme_select(1);assert(selected[2]==0);
 charedit_obj=3;assert(!modern_charedit_scheme_editable());modern_charedit_scheme_select(1);assert(selected[3]==50);
 charedit_obj=1;assert(!modern_charedit_scheme_editable());modern_charedit_scheme_select(1);assert(selected[1]==50);
 charedit_mode=1;assert(modern_charedit_scheme_editable());modern_charedit_scheme_select(1);assert(selected[1]==1);
 pc=0;assert(!modern_charedit_scheme_editable());pc=1;
 charedit_created=false;assert(modern_charedit_scheme_count()==0&&modern_charedit_scheme_id(0)==-1&&!modern_charedit_scheme_editable());
 modern_charedit_scheme_select(2);assert(selected[1]==1);
 puts("PASS: sparse/custom scheme IDs, no-scheme choice, companion-only changes, unrelated NPC and passive-PC guards, closed sheet and invalid indices");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-schemes-') as folder:
    path=Path(folder)
    (path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
