from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
body=(root/'source/src/ui/modern_party_control.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define OBJ_HANDLE_NULL 0
#define INTGAME_MODE_MAIN 0
#define INTGAME_MODE_SKILL 1
#define INTGAME_MODE_SPELL 2
#define INTGAME_MODE_ITEM 3
static int mode;
static int intgame_mode_get(void){return mode;}
static bool intgame_mode_set(int value){mode=value;return true;}
static void dialog_speaker_set(int64_t value){(void)value;}
#define OBJ_TYPE_NPC 1
#define OBJ_F_TYPE 1
#define OBJ_F_CRITTER_FLAGS 2
#define OBJ_F_SPELL_FLAGS 3
#define OCF_FLEEING 1
#define OCF_SPELL_FLEE 2
#define OCF_SURRENDERED 4
#define OSF_MIND_CONTROLLED 8
typedef struct {int64_t id;} ObjectID;
typedef FILE TigFile;
static bool combat,dialog,interruptible=true,alive=true,member=true;
static int interruptions,flags,spells;
static bool obj_handle_is_valid(int64_t obj){return obj>=1&&obj<=3;}
static int64_t player_get_local_pc_obj(void){return 1;}
static int obj_field_int32_get(int64_t obj,int field){
 if(field==OBJ_F_TYPE)return obj==1?0:OBJ_TYPE_NPC;
 return field==OBJ_F_CRITTER_FLAGS?flags:spells;
}
static int64_t critter_pc_leader_get(int64_t obj){return member&&obj==2?1:0;}
static ObjectID obj_get_id(int64_t obj){return (ObjectID){obj};}
static bool objid_is_equal(ObjectID a,ObjectID b){return a.id==b.id;}
static bool combat_turn_based_is_active(void){return combat;}
static bool dialog_ui_is_in_dialog(int64_t obj){(void)obj;return dialog;}
static bool critter_is_active(int64_t obj){return obj&&alive;}
static bool sub_424070(int64_t obj,int a,int b,int c){(void)obj;(void)a;(void)b;(void)c;interruptions++;return interruptible;}
static FILE* tig_file_fopen(const char* path,const char* mode){(void)path;return fopen("party.dat",mode);}
#define tig_file_fread fread
#define tig_file_fwrite fwrite
#define tig_file_fclose fclose
''' + body + r'''
int main(void){
 assert(party_control_load());assert(!modern_party_detached(2));
 assert(!modern_party_set_grouped(3,false));
 assert(modern_party_set_grouped(2,false));assert(interruptions==1&&modern_party_detached(2));
 assert(modern_party_select(2)&&modern_party_selected()==2&&interruptions==1);
 mode=INTGAME_MODE_SKILL;assert(modern_party_select(0)&&mode==INTGAME_MODE_MAIN);
 assert(modern_party_select(2));
 assert(party_control_save());party_detached_count=0;party_selected=0;
 assert(party_control_load()&&modern_party_detached(2)&&!modern_party_selected());
 assert(modern_party_select(2));combat=true;
 assert(!modern_party_selected()&&!modern_party_set_grouped(2,true));
 combat=false;assert(modern_party_set_grouped(2,true)&&!modern_party_detached(2));
 dialog=true;assert(!modern_party_select(2));dialog=false;
 interruptible=false;assert(!modern_party_select(2)&&!modern_party_detached(2));
 interruptible=true;spells=OSF_MIND_CONTROLLED;assert(!modern_party_select(2));
 spells=0;assert(modern_party_select(2));member=false;assert(!modern_party_selected());member=true;
 assert(modern_party_select(2));alive=false;assert(!modern_party_selected());alive=true;
 assert(modern_party_set_grouped(2,true));assert(party_control_save());
 party_detached_count=1;assert(party_control_load()&&!modern_party_detached(2));
 FILE* corrupt=fopen("party.dat","wb");fputs("bad",corrupt);fclose(corrupt);
 assert(!party_control_load()&&party_detached_count==0);
 puts("PASS: grouping, selection, membership, combat/dialog guards and save/load");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-grouping-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True)
