#!/usr/bin/env python3
"""Exercise the engine bubble adapter with real expiration and removal code."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
s = (root / 'source/src/game/tb.c').read_text()
def function(name):
    import re
    match = re.search(r'^(?:void|int|bool) '+name+r'\([^\n]*\)\n\{', s, re.M)
    start = match.start(); end = match.end(); depth = 1
    while depth:
        depth += (s[end] == '{') - (s[end] == '}'); end += 1
    return s[start:end]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAX_TEXT_BUBBLES 8
#define TEXT_BUBBLE_IN_USE 1
#define TEXT_BUBBLE_PERMANENT 2
#define VIEW_TYPE_ISOMETRIC 0
#define OBJ_HANDLE_NULL 0
#define OBJ_F_FLAGS 0
#define OF_TEXT 1
#define TB_EXPIRE_NEVER -2
#define FREE free
typedef int tig_timestamp_t;
typedef struct {int x,y,width,height;} TigRect;
typedef struct {int flags;int64_t obj;int timestamp,duration;TigRect rect;char* text;int type;} TextBubble;
typedef struct {const char* text;int type,x,y,width,height;} ModernBubble;
static TextBubble tb_text_bubbles[8];
static bool tb_enabled=true;
static struct {int type;} tb_view_options;
static TigRect tb_iso_content_rect={0,0,1280,720};
static int invalidations,gamelib_ping_time,tb_text_duration=6000;
static int64_t dialog_actor;
static int64_t modern_dialog_actor(void){return dialog_actor;}
static void tb_get_rect(TextBubble* b,TigRect* r){*r=b->rect;}
static int obj_field_int32_get(int64_t o,int f){(void)o;(void)f;return 1;}
static void obj_field_int32_set(int64_t o,int f,int v){(void)o;(void)f;(void)v;}
static void invalidate(TigRect* r){(void)r;invalidations++;}
static void (*tb_iso_window_invalidate_rect)(TigRect*)=invalidate;
static int tig_timer_between(int a,int b){return b-a;}
static TextBubble* find_text_bubble(int64_t o){for(int i=0;i<8;i++)if(tb_text_bubbles[i].flags&1 && tb_text_bubbles[i].obj==o)return &tb_text_bubbles[i];return NULL;}
''' + '\n'.join(function(n) for n in ['tb_remove_internal','tb_expire_in','tb_ping','modern_bubbles','modern_bubbles_sync']) + r'''
static void add(int i,int64_t obj,const char* text,int x){
 TextBubble* b=&tb_text_bubbles[i];
 *b=(TextBubble){.flags=1,.obj=obj,.duration=6000,.rect={x,200,200,100},.type=i%4};
 b->text=malloc(strlen(text)+1);strcpy(b->text,text);
}
int main(void){
 ModernBubble result[8];
 add(0,1,"Original localized text",100);add(1,2,"Other speaker",400);add(2,3,"Off screen",1400);
 assert(modern_bubbles(result,8)==2);assert(strcmp(result[0].text,"Original localized text")==0);
 assert(modern_bubbles(result,1)==1);
 dialog_actor=1;assert(modern_bubbles(result,8)==1 && result[0].type==1);dialog_actor=0;
 tb_enabled=false;assert(modern_bubbles(result,8)==0);tb_enabled=true;
 tb_expire_in(2,TB_EXPIRE_NEVER);tb_ping(6001);
 assert(modern_bubbles(result,8)==1 && strcmp(result[0].text,"Other speaker")==0);
 assert(tb_text_bubbles[0].text==NULL && tb_text_bubbles[2].text==NULL);
 gamelib_ping_time=6001;tb_expire_in(2,2);tb_ping(8000);assert(modern_bubbles(result,8)==1);
 tb_ping(8002);assert(modern_bubbles(result,8)==0 && tb_text_bubbles[1].text==NULL);
 int before=invalidations;modern_bubbles_sync(true);assert(invalidations==before+1);
 modern_bubbles_sync(true);assert(invalidations==before+1);
 modern_bubbles_sync(false);assert(invalidations==before+2);
 puts("PASS: original text, multiple speakers, dialogue exclusion, expiration, permanent lines and F9 invalidation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-bubbles-test-') as directory:
    c = Path(directory) / 'test.c'; exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
