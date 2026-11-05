"""Exercise creation transactions using the actual editor action code."""
import os
from pathlib import Path
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
implementation=(root/'source/src/ui/modern_background.inc').read_text(encoding='utf-8').split('static void draw_background_history')[0]
implementation=implementation.replace('#include "modern_background_loadout.inc"','')
implementation=implementation.replace('#include "modern_background_choices.inc"','')
source=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
enum { STAT_STRENGTH,STAT_CONSTITUTION,STAT_DEXTERITY,STAT_BEAUTY,
       STAT_INTELLIGENCE,STAT_WILLPOWER,STAT_PERCEPTION,STAT_CHARISMA,
       STAT_SPEED,STAT_MAX_FOLLOWERS };
#define MM_WINDOW_NEW_CHAR 1
typedef struct { int64_t object; } ModernCharacter;
typedef struct { float x,y,w,h; } SDL_FRect;
static int actor_values[8]={8,8,8,8,8,8,8,8},mode=1;
static int bonuses[2];
static const int modern_background_bonus_stats[2]={STAT_SPEED,STAT_MAX_FOLLOWERS};
static bool modern_background_bonuses_available(void){return true;}
static int modern_background_bonus_get(int64_t o,int i){(void)o;return bonuses[i];}
static void modern_background_bonus_set(int64_t o,int i,int value){(void)o;bonuses[i]=value;}
static void modern_background_preview(int64_t o,const int primary[8],const int draft[2],int result[16]){
 (void)o;memset(result,0,16*sizeof(int));memcpy(result,primary,8*sizeof(int));
 result[STAT_SPEED]=primary[STAT_DEXTERITY]+draft[0];result[STAT_MAX_FOLLOWERS]=primary[STAT_CHARISMA]/4+draft[1];
}
static int64_t actor=1;
static int window;
#define tr(a,b) (b)
static void SDL_strlcpy(char* a,const char* b,size_t n){assert(strlen(b)<n);memcpy(a,b,strlen(b)+1);}
static bool disk_ok=true;
static char background_title[96],background_story[1024];
static char background_money[11],background_item_search[96];
static int background_item_page,background_item_pages,background_item_rows[7],background_quantities[16066];
static bool background_equipment;
static int loadout_commits;
static void background_loadout_open(int64_t o){(void)o;}
static void background_loadout_commit(void){loadout_commits++;}
static bool background_item_allowed(int id){return id==6029;}
static bool background_money_value(const char* text,int* v){*v=0;for(const char* p=text;*p;p++){if(*p<'0'||*p>'9'||*v>(INT_MAX-(*p-'0'))/10)return false;*v=*v*10+(*p-'0');}return true;}
static int stat_base_get(int64_t o,int s){(void)o;return actor_values[s];}
static int stat_base_set(int64_t o,int s,int v){(void)o;actor_values[s]=v;return v;}
static int stat_level_min(int64_t o,int s){(void)o;(void)s;return 1;}
static int stat_level_max(int64_t o,int s){(void)o;(void)s;return 20;}
static int mainmenu_ui_window_type_get(void){return mode;}
static bool modern_mainmenu_character(ModernCharacter* c,int tab){(void)tab;c->object=actor;return true;}
static void SDL_StartTextInput(int w){(void)w;}
static void SDL_StopTextInput(int w){(void)w;}
static void SDL_strlcat(char* a,const char* b,size_t n){assert(strlen(a)+strlen(b)<n);memcpy(a+strlen(a),b,strlen(b)+1);}
static bool background_profile_read(int64_t o,char* t,size_t tn,char* s,size_t sn){(void)o;(void)tn;(void)sn;t[0]=s[0]=0;return false;}
static struct { int64_t object;int selected; } background_choice_state={1,-1};
static int background_get(int64_t o){(void)o;return 0;}
static int background_text_get(int64_t o){(void)o;return 0;}
static const char* background_description_get_name(int n){(void)n;return "Native";}
static const char* background_description_get_body(int n){(void)n;return "Native story";}
static const char* game_text(const char* s){return s;}
static bool background_choice_commit(int64_t o,const int d[8],const int b[2]){(void)o;(void)d;(void)b;return disk_ok;}
IMPLEMENTATION
int main(void){
 background_editor_open(1);background_editor_action(571);
 assert(background_draft_valid(1));
 for(int i=0;i<30;i++)background_editor_action(571);
 assert(background_draft[0]==12);background_editor_action(587);assert(actor_values[0]==20 && loadout_commits==1);
 background_editor_open(1);for(int i=0;i<30;i++)background_editor_action(570);
 assert(background_draft[0]==-19);background_editor_action(587);assert(actor_values[0]==1);
 actor_values[0]=8; background_editor_open(1);background_editor_action(571);
 background_editor_action(584);assert(background_draft_valid(1));
 background_editor_action(586);assert(actor_values[0]==8 && actor_values[7]==8);
 background_editor_open(1);background_editor_action(571);background_editor_action(584);
 disk_ok=false;background_editor_action(587);assert(actor_values[0]==8 && actor_values[7]==8 && background_editor);
 disk_ok=true;background_editor_action(587);assert(actor_values[0]==9 && actor_values[7]==7 && !background_editor);
 /* A queued action cannot modify a different newly created actor. */
 background_editor_open(1);actor=2;background_editor_action(571);assert(!background_editor);
 actor=1;background_editor_open(1);mode=0;background_editor_action(587);assert(!background_editor);
 mode=1;background_editor_open(1);background_editor_action(589);background_text_replace=true;
 background_editor_text("A\xc3\xa9",false);background_editor_text("",true);assert(!strcmp(background_title,"A"));
 /* Rejected replacement input must preserve the previous text and selection. */
 char oversized[1100];memset(oversized,'x',sizeof(oversized)-1);oversized[sizeof(oversized)-1]=0;
 background_text_replace=true;background_editor_text(oversized,false);
 assert(!strcmp(background_title,"A") && background_text_replace);
 background_editor_text("",false);assert(!strcmp(background_title,"A") && background_text_replace);
 background_editor_text("line\nline",false);assert(!strcmp(background_title,"A"));
 background_editor_text("New",false);assert(!strcmp(background_title,"New"));
 background_editor_action(590);background_editor_text("First\nSecond",false);
 assert(!strcmp(background_story,"First\nSecond"));
 background_text_replace=true;background_editor_text("First\r\nSecond\rThird",false);
 assert(!strcmp(background_story,"First\nSecond\nThird"));
 /* Capacity applies after newline normalization, without partial pastes. */
 char paragraphs[1201];for(int i=0;i<400;i++){paragraphs[i*3]='x';paragraphs[i*3+1]='\r';paragraphs[i*3+2]='\n';}paragraphs[1200]=0;
 background_text_replace=true;background_editor_text(paragraphs,false);assert(strlen(background_story)==800);
 background_text_replace=true;background_editor_text(oversized,false);assert(strlen(background_story)==800 && background_text_replace);
 background_editor_action(589);background_text_replace=true;background_editor_text("",true);
 assert(!background_draft_valid(1));background_editor_action(587);assert(background_editor);
 background_editor_action(586);
 (void)background_story_rect;
 background_editor_open(1);background_editor_action(595);background_text_replace=true;
 background_editor_text("2147483647",false);assert(!strcmp(background_money,"2147483647"));
 background_text_replace=true;background_editor_text("2147483648",false);assert(!strcmp(background_money,"2147483647") && background_text_replace);
 background_editor_text("-1",false);assert(background_text_replace);
 background_editor_text("0",false);assert(!strcmp(background_money,"0"));
 background_editor_action(594);background_item_rows[0]=6029;
 background_editor_action(601);assert(background_quantities[6029]==1);
 background_editor_action(600);assert(background_quantities[6029]==0);
 background_editor_action(600);assert(background_quantities[6029]==0);
 background_editor_action(586);
 /* Derived changes remain drafts until the complete transaction succeeds. */
 background_editor_open(1);background_editor_action(599);background_editor_action(615);background_editor_action(617);
 assert(background_bonuses[0]==1 && background_bonuses[1]==1 && bonuses[0]==0 && bonuses[1]==0);
 background_editor_action(586);assert(bonuses[0]==0 && bonuses[1]==0);
 background_editor_open(1);background_editor_action(599);background_editor_action(615);
 disk_ok=false;background_editor_action(587);assert(bonuses[0]==0 && background_editor);
 disk_ok=true;background_editor_action(587);assert(bonuses[0]==1 && !background_editor);
 background_editor_open(1);assert(background_bonuses[0]==1);background_editor_action(599);
 background_editor_action(614);background_editor_action(587);assert(bonuses[0]==0);
 puts("PASS: free attributes, native bounds, cancellation, disk rollback, stale actor, UTF-8, numeric money and item actions");
}
'''.replace('IMPLEMENTATION',implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-background-') as tmp:
    code=Path(tmp)/'test.c'; binary=Path(tmp)/'test'
    code.write_text(source,encoding='utf-8')
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(code),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
