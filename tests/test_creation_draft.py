"""Check first-page preservation without carrying point allocations backwards."""
import os
import re
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
implementation = (root / 'source/src/ui/modern_creation_draft.inc').read_text(encoding='utf-8')
stat_enum = re.search(r'typedef enum Stat \{.*?\} Stat;', (root / 'source/src/game/stat.h').read_text(), re.S).group()
source = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
STAT_ENUM
enum { OBJ_F_CURRENT_AID, OBJ_F_CRITTER_STAT_BASE_IDX, OBJ_F_CRITTER_PORTRAIT, OBJ_F_PC_PLAYER_NAME };
#define OBJ_HANDLE_NULL 0
static char mainmenu_ui_textedit_buffer[24];
static struct { int body_type; } stru_5C5170[10];
static struct { int stats[STAT_COUNT], portrait, bg, text, points, skills, bonuses[2]; char name[24]; } actors[4];
static bool modern_background_bonuses_available(void){return true;}
static int modern_background_bonus_get(int64_t o,int i){return actors[o].bonuses[i];}
static void modern_background_bonus_set(int64_t o,int i,int value){actors[o].bonuses[i]=value;}
static int stat_base_get(int64_t o,int s){return actors[o].stats[s];}
static int stat_level_get(int64_t o,int s){return actors[o].stats[s];}
static int portrait_get(int64_t o){return actors[o].portrait;}
static int background_get(int64_t o){return actors[o].bg;}
static int background_text_get(int64_t o){return actors[o].text;}
static bool obj_handle_is_valid(int64_t o){return o>0 && o<4;}
static void SDL_strlcpy(char* a,const char* b,size_t n){assert(strlen(b)<n);memcpy(a,b,strlen(b)+1);}
static void object_set_gender_and_race(int64_t o,int body,int gender,int race){(void)body;actors[o].stats[STAT_RACE]=race;actors[o].stats[STAT_GENDER]=gender;}
static int obj_field_int32_get(int64_t o,int f){(void)o;(void)f;return 0;}
static void object_set_current_aid(int64_t o,int aid){(void)o;(void)aid;}
static void background_clear(int64_t o){actors[o].bg=actors[o].text=0;}
static void background_set(int64_t o,int bg,int text){actors[o].bg=bg;actors[o].text=text;}
static void obj_arrayfield_int32_set(int64_t o,int f,int s,int v){assert(f==OBJ_F_CRITTER_STAT_BASE_IDX);actors[o].stats[s]=v;}
static void obj_field_int32_set(int64_t o,int f,int v){assert(f==OBJ_F_CRITTER_PORTRAIT);actors[o].portrait=v;}
static void obj_field_string_set(int64_t o,int f,const char* name){assert(f==OBJ_F_PC_PLAYER_NAME);SDL_strlcpy(actors[o].name,name,sizeof(actors[o].name));}
static void ui_spell_maintain_refresh(void){}
IMPLEMENTATION
int main(void){
 for(int i=0;i<8;i++)actors[1].stats[i]=8;
 actors[1].stats[0]=9;actors[1].stats[7]=7;
 actors[1].stats[STAT_RACE]=2;actors[1].stats[STAT_GENDER]=1;
 actors[1].bg=17;actors[1].text=1017;actors[1].portrait=42;
 actors[1].bonuses[0]=3;actors[1].bonuses[1]=-1;
 strcpy(mainmenu_ui_textedit_buffer,"Draft");modern_creation_draft_capture(1);
 /* Allocations made on the second page must not become the next refund floor. */
 actors[1].stats[0]=12;actors[1].skills=2;actors[1].points=0;
 modern_creation_draft.resume=true;assert(modern_creation_draft_take(1));
 actors[2].points=5;modern_creation_draft_restore(2);
 assert(actors[2].stats[0]==9 && actors[2].stats[7]==7);
 assert(actors[2].stats[STAT_RACE]==2 && actors[2].stats[STAT_GENDER]==1);
 assert(actors[2].bg==17 && actors[2].text==1017 && actors[2].portrait==42);
 assert(!strcmp(actors[2].name,"Draft") && !strcmp(mainmenu_ui_textedit_buffer,"Draft"));
 assert(actors[2].skills==0 && actors[2].points==5);
 assert(actors[2].bonuses[0]==3 && actors[2].bonuses[1]==-1);
 /* Consume once; a fresh character or stale handle cannot inherit a draft. */
 assert(!modern_creation_draft_take(2));
 modern_creation_draft_capture(2);modern_creation_draft.resume=true;assert(!modern_creation_draft_take(3));
 modern_creation_draft_capture(2);assert(!modern_creation_draft_take(2));
 modern_creation_draft_capture(2);modern_creation_draft.resume=true;assert(!modern_creation_draft_take(0));
 /* Exact base values survive, including values above a human's nominal cap. */
 actors[2].stats[0]=24;modern_creation_draft_capture(2);modern_creation_draft.resume=true;
 assert(modern_creation_draft_take(2));modern_creation_draft_restore(3);assert(actors[3].stats[0]==24);
 puts("PASS: creation draft, native choices, no carried allocations, stale/fresh guards, exact raw values");
}
'''.replace('IMPLEMENTATION', implementation).replace('STAT_ENUM', stat_enum)
with tempfile.TemporaryDirectory(prefix='renaissance-creation-draft-') as tmp:
    code = Path(tmp) / 'test.c'
    binary = Path(tmp) / 'test'
    code.write_text(source, encoding='utf-8')
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror', str(code), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
