#!/usr/bin/env python3
"""Check native window anchors, visible repaint and child hitboxes on real code."""
from pathlib import Path
import os, re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
window=(root/'source/first_party/tig/src/window.c').read_text()
button=(root/'source/first_party/tig/src/button.c').read_text()
header=(root/'source/first_party/tig/include/tig/window.h').read_text()
def function(source, signature):
    start=source.index(signature+'\n{')
    return source[start:source.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#define TIG_WINDOW_MAX 5
#define MAX_BUTTONS 4
#define TIG_WINDOW_USAGE_FREE 1
#define TIG_BUTTON_USAGE_FREE 1
#define TIG_VIDEO_BUFFER_CREATE_VIDEO_MEMORY 1
#define TIG_OK 0
#define TIG_ERR_NOT_INITIALIZED 1
#define TIG_ERR_INVALID_PARAM 2
#define TIG_WINDOW_HANDLE_INVALID -1
#define TIG_WINDOW_HIDDEN 0x20
#define TIG_WINDOW_TRANSPARENT 1
typedef int tig_window_handle_t;
typedef int tig_button_handle_t;
typedef unsigned TigWindowFlags;
typedef struct {int x,y,width,height;} TigRect;
typedef struct {int unused;} TigVideoBuffer;
typedef struct {int flags,width,height,background_color;} TigVideoBufferCreateInfo;
typedef struct Node {struct Node* next;} TigRectListNode;
typedef struct {int usage,flags;TigRect frame,bounds;int background_color,num_buttons;int buttons[2];TigVideoBuffer* video_buffer;} TigWindow;
typedef struct {int usage;TigRect rect;} TigButton;
static TigWindow windows[TIG_WINDOW_MAX];
static TigButton buttons[MAX_BUTTONS];
static bool tig_window_initialized=true,fail_buffer,fail_video;
static TigRect tig_window_screen_rect={0,0,1360,900};
static TigRectListNode* tig_window_dirty_rects;
static int invalidations,mouse_width,mouse_height;
static int tig_window_handle_to_index(int h){return h;}
static int tig_window_index_to_handle(int h){return h;}
static int tig_button_handle_to_index(int h){return h;}
static void tig_window_invalidate_rect(TigRect* r){(void)r;invalidations++;}
static void tig_rect_node_destroy(TigRectListNode* p){free(p);}
static void tig_mouse_resize_screen(int w,int h){mouse_width=w;mouse_height=h;}
static int tig_video_buffer_create(TigVideoBufferCreateInfo* info,TigVideoBuffer** out){(void)info;if(fail_buffer)return 2;*out=calloc(1,sizeof(**out));return 0;}
static void tig_video_buffer_destroy(TigVideoBuffer* p){free(p);}
static int tig_video_resize_logical(int w,int h){(void)w;(void)h;return fail_video?2:0;}
'''
code+='\n'.join(re.findall(r'^#define TIG_WINDOW_ANCHOR_.*$',header,re.M))+'\n'
code+=function(button,'int tig_button_move(tig_button_handle_t button_handle, int dx, int dy)')+'\n'
for signature in ['int tig_window_move(tig_window_handle_t window_handle, int x, int y)',
                  'int tig_window_set_anchor(tig_window_handle_t window_handle, TigWindowFlags anchor)',
                  'int tig_window_resize_world(tig_window_handle_t handle, int width, int height)']:
    code+=function(window,signature)+'\n'
code+=r'''
static void positions(int w,int h){
 int x=(w-800)/2,y=(h-600)/2;
 assert(windows[1].frame.x==x&&windows[1].frame.y==y+41);
 assert(buttons[0].rect.x==x+20&&buttons[0].rect.y==y+51);
 assert(windows[2].frame.x==x&&windows[2].frame.y==h-159);
 assert(buttons[1].rect.x==x+40&&buttons[1].rect.y==h-139);
 assert(windows[3].frame.x==x+503&&windows[3].frame.y==y+104);
 assert(windows[3].flags&TIG_WINDOW_HIDDEN);
 assert(windows[4].frame.x==23&&windows[4].frame.y==37); /* Dedicated viewport client untouched. */
 assert(mouse_width==w&&mouse_height==h);
}
int main(void){
 windows[0].frame=windows[0].bounds=tig_window_screen_rect;
 windows[1]=(TigWindow){.frame={280,191,800,400},.flags=TIG_WINDOW_ANCHOR_CENTER_X|TIG_WINDOW_ANCHOR_CENTER_Y,.num_buttons=1,.buttons={0}};
 windows[2]=(TigWindow){.frame={280,741,800,159},.flags=TIG_WINDOW_ANCHOR_CENTER_X|TIG_WINDOW_ANCHOR_BOTTOM,.num_buttons=1,.buttons={1}};
 windows[3]=(TigWindow){.frame={783,254,234,326},.flags=TIG_WINDOW_ANCHOR_CENTER_X|TIG_WINDOW_ANCHOR_CENTER_Y|TIG_WINDOW_HIDDEN};
 windows[4].frame=(TigRect){23,37,50,50};
 buttons[0].rect=(TigRect){300,201,30,20};buttons[1].rect=(TigRect){320,761,30,20};
 int sizes[][2]={{1601,1001},{800,600},{1920,1080},{1360,900},{1361,901},{1360,900}};
 for(int i=0;i<6;i++){assert(tig_window_resize_world(0,sizes[i][0],sizes[i][1])==TIG_OK);positions(sizes[i][0],sizes[i][1]);}
 fail_buffer=true;assert(tig_window_resize_world(0,1600,1000)!=TIG_OK);positions(1360,900);fail_buffer=false;
 fail_video=true;assert(tig_window_resize_world(0,1600,1000)!=TIG_OK);positions(1360,900);fail_video=false;
 invalidations=0;assert(tig_window_move(1,300,211)==TIG_OK&&invalidations==2);
 assert(buttons[0].rect.x==320&&buttons[0].rect.y==221);
 invalidations=0;assert(tig_window_move(3,800,280)==TIG_OK&&invalidations==0);
 assert(tig_window_move(-1,0,0)==TIG_ERR_INVALID_PARAM);
 windows[4].usage=TIG_WINDOW_USAGE_FREE;assert(tig_window_move(4,0,0)==TIG_ERR_INVALID_PARAM);
 assert(tig_window_set_anchor(2,TIG_WINDOW_ANCHOR_CENTER_Y)==TIG_OK);
 assert((windows[2].flags&TIG_WINDOW_ANCHOR_MASK)==TIG_WINDOW_ANCHOR_CENTER_Y);
 tig_video_buffer_destroy(windows[0].video_buffer);
 puts("PASS: centered/bottom anchors, odd-size round trips, hidden windows, child hitboxes, repaint, allocation rollback and unchanged viewport clients");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-native-layout-') as d:
    p=Path(d);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
