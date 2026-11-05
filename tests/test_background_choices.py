"""Run the real catalogue and selection code with native character boundaries."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
implementation = (root / 'source/src/ui/modern_background_choices.inc').read_text(encoding='utf-8')
source = r'''
#define _CRT_SECURE_NO_WARNINGS
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#define BACKGROUND_ITEM_LIMIT 16066
#define OBJ_F_CRITTER_STAT_BASE_IDX 42
static int stats[8]={8,8,8,8,8,8,8,8},native,bonus[2];
static bool rename_ok=true,restricted,effects_ok=true;
static char background_title[96],background_story[1024],background_money[11];
static int background_quantities[BACKGROUND_ITEM_LIMIT];
static struct {int64_t object;char money[11];int quantities[BACKGROUND_ITEM_LIMIT];bool failed;} background_loadout;
static struct {bool active;int64_t object;char title[96],story[1024];} background_pending_profile;
static void SDL_strlcpy(char* a,const char* b,size_t n){assert(strlen(b)<n);memcpy(a,b,strlen(b)+1);}
static bool SDL_RenamePath(const char* from,const char* to){if(!rename_ok)return false;remove(to);return rename(from,to)==0;}
static bool background_money_value(const char* t,int* v){*v=0;for(;*t;t++){if(*t<'0'||*t>'9'||*v>(INT_MAX-(*t-'0'))/10)return false;*v=*v*10+*t-'0';}return true;}
static bool modern_background_creation_ready(int64_t obj){assert(obj==background_pending_profile.object);return rename_ok;}
static int stat_base_get(int64_t obj,int stat){(void)obj;return stats[stat];}
static int stat_base_set(int64_t obj,int stat,int value){(void)obj;if(value>20)value=20;if(value<1)value=1;return stats[stat]=value;}
static void obj_arrayfield_int32_set(int64_t obj,int field,int stat,int value){(void)obj;assert(field==42);stats[stat]=value;}
static void modern_background_bonus_set(int64_t obj,int which,int value){(void)obj;bonus[which]=value;}
static bool modern_background_bonuses_available(void){return effects_ok;}
static bool background_item_allowed(int id){return id==6029 || id==7038;}
static void modern_background_loadout_begin(int64_t prev,int64_t obj){(void)prev;memset(&background_loadout,0,sizeof(background_loadout));background_loadout.object=obj;}
static void background_clear(int64_t obj){(void)obj;native=0;}
static void background_set(int64_t obj,int base,int text){(void)obj;assert(text==1000+base);assert(base>=0&&base<=2);native=base;}
static int background_get_description(int base){return base+1000;}
static int background_get(int64_t obj){(void)obj;return native;}
static bool background_find_first(int64_t obj,int* base){(void)obj;*base=0;return true;}
static bool background_find_next(int64_t obj,int* base){(void)obj;int n=*base+1;if(restricted&&n==1)n++;if(n>2)return false;*base=n;return true;}
static bool background_find_prev(int64_t obj,int* base){(void)obj;int n=*base-1;if(restricted&&n==1)n--;if(n<0)return false;*base=n;return true;}
IMPLEMENTATION
static void forget(void){free(background_choices);background_choices=NULL;background_choice_count=0;background_choices_loaded=background_choices_failed=false;}
static void draft(const char* name){strcpy(background_title,name);strcpy(background_story,"First\nSecond");strcpy(background_money,"12345");background_quantities[6029]=2;background_quantities[7038]=10;}
int main(void){
 modern_background_loadout_begin(0,1);modern_background_choices_begin(0,1);
 assert(!modern_background_choice_cycle(1,1));
 /* Copy original 1; the engine ID never becomes a catalogue index. */
 native=1;draft("Tarant scholar");int delta[8]={2,0,0,0,0,0,0,-1},bonuses[2]={2,-1};
 stats[0]=10;stats[7]=7;assert(background_choice_commit(1,delta,bonuses));
 background_choice_select(1,1,0);assert(stats[0]==10&&stats[7]==7&&bonus[0]==2&&native==1);
 assert(background_choice_count==1&&!strcmp(background_pending_profile.title,"Tarant scholar"));
 assert(background_loadout.quantities[6029]==2&&!strcmp(background_loadout.money,"12345"));
 /* Left reaches the untouched original; right restores the copy exactly. */
 assert(modern_background_choice_cycle(1,-1));assert(native==1&&stats[0]==8&&stats[7]==8&&bonus[0]==0);
 assert(!*background_pending_profile.title&&!background_loadout.quantities[6029]&&!*background_loadout.money);
 assert(modern_background_choice_cycle(1,1));assert(native==1&&stats[0]==10&&bonus[1]==-1);
 /* Edit in place. Disk failure cannot create a duplicate or replace the entry. */
 draft("Revised scholar");delta[0]=1;delta[7]=0;stats[0]++;
 rename_ok=false;assert(!background_choice_commit(1,delta,bonuses));assert(background_choice_count==1&&!strcmp(background_choices[0].title,"Tarant scholar"));
 rename_ok=true;assert(background_choice_commit(1,delta,bonuses));assert(background_choice_count==1&&background_choices[0].primary[0]==3);
 modern_background_choice_cycle(1,-1);assert(stats[0]==8);modern_background_choice_cycle(1,1);assert(stats[0]==11);
 /* Back rebinds the selection to the restored object without reapplying it. */
 modern_background_choices_begin(1,2);background_loadout.object=2;
 modern_background_choice_cycle(2,-1);assert(stats[0]==8);modern_background_choice_cycle(2,1);assert(stats[0]==11);
 modern_background_choice_leave(2);assert(stats[0]==8&&bonus[0]==0);
 /* A second original copy remains a distinct list entry. */
 native=1;draft("Second copy");memset(delta,0,sizeof(delta));assert(background_choice_commit(2,delta,bonuses));
 assert(background_choice_count==2&&background_choice_state.selected==1);
 modern_background_choice_cycle(2,-1);assert(background_choice_state.selected==0&&stats[0]==11);
 modern_background_choice_cycle(2,1);assert(background_choice_state.selected==1&&stats[0]==8);
 modern_background_choice_cycle(2,1);assert(native==2&&background_choice_state.selected==-1);
 modern_background_choice_cycle(2,1);assert(native==0);modern_background_choice_cycle(2,-1);assert(native==2);
 /* Reopen the catalogue: complete definitions survive a process restart. */
 forget();modern_background_choices_begin(0,3);background_choices_load();
 assert(background_choice_count==2&&!background_choices_failed&&!strcmp(background_choices[0].title,"Revised scholar"));
 assert(background_choices[0].quantities[7038]==10&&background_choices[0].bonuses[1]==-1);
 native=1;modern_background_loadout_begin(0,3);modern_background_choice_cycle(3,1);assert(stats[0]==11);
 modern_background_choice_leave(3);stats[0]=19;modern_background_choice_cycle(3,1);assert(stats[0]==20);
 modern_background_choice_leave(3);assert(stats[0]==19);stats[0]=8;
 /* Native eligibility and effect availability also govern copies. */
 native=0;restricted=true;modern_background_choice_cycle(3,1);assert(native==2);
 restricted=false;native=1;effects_ok=false;modern_background_choice_cycle(3,1);assert(native==2);
 effects_ok=true;
 /* Truncated files are rejected as a whole and never overwritten by Apply. */
 forget();FILE* f=fopen("modern-ui/backgrounds.dat","wb");assert(f);fputs("ARCBG01\n\x01",f);fclose(f);
 background_choices_load();assert(background_choices_failed&&background_choice_count==0);
 assert(!background_choice_commit(3,delta,bonuses));
 forget();puts("PASS: copy/edit, atomic failure, independent stats/kit/prose, native IDs, arrows/wrap, Back, reload, caps, eligibility, corrupt catalogue");
}
'''.replace('IMPLEMENTATION', implementation)
with tempfile.TemporaryDirectory(prefix='renaissance-background-choices-') as tmp:
    code=Path(tmp)/'test.c';binary=Path(tmp)/'test'
    (Path(tmp)/'modern-ui').mkdir()
    code.write_text(source,encoding='utf-8')
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(code),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],cwd=tmp,check=True)
