#!/usr/bin/env python3
"""Personal annotations retain town identity across storage, rendering and editing."""
from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_notebook.inc').read_text();s=s[:s.index('static int journal_mark_index')]
panels=(root/'source/src/ui/modern_panels.inc').read_text()
def fn(signature):
 start=panels.index(signature+'\n{');return panels[start:panels.index('\n}',start)+2]+'\n'
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
typedef uint64_t Uint64;
#define OID_STR_LEN 64
#define SDL_isalnum isalnum
static Uint64 ticks=1000;
static Uint64 SDL_GetTicks(void){return ticks;}
static bool SDL_RenamePath(const char* from,const char* to){return rename(from,to)==0;}
static void SDL_strlcpy(char* to,const char* from,size_t n){snprintf(to,n,"%s",from);}
static int64_t player_get_local_pc_obj(void){return 1;}
static bool obj_handle_is_valid(int64_t obj){return obj==1;}
static int obj_get_id(int64_t obj){return (int)obj;}
static void objid_id_to_str(char* out,int id){(void)id;strcpy(out,"fixture");}
static bool dirty;
typedef struct{int mode,map,townmap,offset_x,offset_y;}ModernMap;
''' +s+r'''
typedef struct{float x,y,w,h;}SDL_FRect;
static SDL_FRect memo_map_rect,memo_hover_rect;
static ModernMap memo_map_state;
static int memo_hover,buttons,last_button;
static float mouse_x,mouse_y;
static bool inside(SDL_FRect r,float x,float y){return x>=r.x&&y>=r.y&&x<r.x+r.w&&y<r.y+r.h;}
static void button(SDL_FRect r,int action,const char* text,int icon,bool active,bool allowed){(void)r;(void)text;(void)icon;(void)active;assert(allowed);buttons++;last_button=action;}
static void memo_focus(int i){memo_edit=i;}
static void memo_close(void){memo_edit=-1;}
'''+fn('static void draw_map_memos(ModernMap* map,SDL_FRect dest,float scale)')+fn('static bool memo_action(int action)')+r'''
int main(void){
 FILE* f=fopen("modern-ui/notebook-fixture.txt","w");assert(f);
 fputs("TRACK 8\nFAV 2 12\nNOTE 2 7 50 50 Ancienne note Éaux\nNOTE2 2 7 101 50 50 Ville A\nNOTE2 2 7 205 50 50 Ville B\nNOTE 0 0 50 50 Monde\n",f);fclose(f);
 notebook_sync();assert(map_memo_count==4&&journal_mark_count==1&&tracked_quest==8);
 ModernMap a={.mode=2,.map=7,.townmap=101},b={.mode=2,.map=7,.townmap=205},c={.mode=2,.map=7,.townmap=309};
 assert(!map_memo_matches(&map_memos[0],&a)&&!map_memo_matches(&map_memos[0],&b));
 assert(map_memo_matches(&map_memos[1],&a)&&!map_memo_matches(&map_memos[1],&b));
 assert(map_memo_matches(&map_memos[2],&b)&&!map_memo_matches(&map_memos[2],&a));
 SDL_FRect rect={0,0,100,100};mouse_x=mouse_y=50;
 draw_map_memos(&a,rect,1);assert(buttons==1&&last_button==2201&&memo_hover==1);
 buttons=0;draw_map_memos(&b,rect,1);assert(buttons==1&&last_button==2202&&memo_hover==2);
 buttons=0;draw_map_memos(&c,rect,1);assert(!buttons&&memo_hover==-1);
 assert(memo_action(2201)&&memo_edit==-1); /* stale foreign click */
 draw_map_memos(&a,rect,1);assert(memo_action(2201)&&memo_edit==1);
 strcpy(memo_draft.text,"Ville A modifiée");assert(memo_action(2192));assert(map_memos[1].townmap==101);
 assert(notebook_save());notebook_identity[0]=0;notebook_sync();assert(map_memo_count==4);
 assert(!map_memos[0].townmap&&!strcmp(map_memos[0].text,"Ancienne note Éaux"));
 assert(map_memos[1].townmap==101&&!strcmp(map_memos[1].text,"Ville A modifiée"));assert(map_memos[2].townmap==205);
 assert(journal_mark_count==1&&journal_marks[0].id==12&&tracked_quest==8);
 ModernMap world={.mode=0,.map=0};assert(map_memo_matches(&map_memos[3],&world));
 f=fopen("ArcanumModern/notebook-notes.log","wb");assert(f);fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);ticks+=2000;
 for(int i=0;i<100;i++){errno=EDOM;notebook_notes_log("test",true);assert(errno==EDOM);}
 puts("PASS: personal notes scoped by town within shared world map, legacy notes preserved without guessing, NOTE2 round-trip, favorites/tracking, rendered buttons/hover/edit scope and diagnostics");
}
'''
assert '.townmap=memo_map_state.townmap' in panels
with tempfile.TemporaryDirectory(prefix='arcanum-notebook-scope-') as folder:
 p=Path(folder);(p/'modern-ui').mkdir();(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Wno-unused-variable','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/notebook-notes.previous.log').stat().st_size==1024*1024
 log=(p/'ArcanumModern/notebook-notes.log').read_text();assert len(log.splitlines())==20 and 'Ville' not in log and 'unassigned=1' in log
