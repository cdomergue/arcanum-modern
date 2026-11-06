"""Compile the production clipper/rasterizer and check guarded pixel buffers."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
rect = (root / 'source/first_party/tig/src/rect.c').read_text()
video = (root / 'source/first_party/tig/src/video.c').read_text()

def function(source, signature):
    start = source.index(signature + '\n{')
    brace = source.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint32_t tig_color_t;
typedef struct { int x,y,width,height; } TigRect;
typedef struct { int x1,y1,x2,y2; } TigLine;
typedef struct { int mode,thickness; } TigDrawLineModeInfo;
typedef struct { int style; } TigDrawLineStyleInfo;
typedef struct { int w,h,pitch; void* pixels; } Surface;
typedef struct { Surface* surface; } TigVideoBuffer;
enum { TIG_OK, TIG_ERR_GENERIC, TIG_ERR_DIRECTX, TIG_ERR_NO_INTERSECTION,
       TIG_LINE_STYLE_SOLID, TIG_LINE_STYLE_DOTTED, TIG_LINE_STYLE_DASHED };
#define MISS_BOTTOM 8
#define MISS_TOP 4
#define MISS_RIGHT 2
#define MISS_LEFT 1
static int tig_video_bpp=32, style=TIG_LINE_STYLE_DASHED, locks;
static void tig_draw_get_line_mode(TigDrawLineModeInfo* p){p->mode=0;p->thickness=1;}
static void tig_draw_get_line_style(TigDrawLineStyleInfo* p){p->style=style;}
static int tig_video_buffer_lock(TigVideoBuffer* p){(void)p;locks++;return TIG_OK;}
static int tig_video_buffer_unlock(TigVideoBuffer* p){(void)p;locks--;return TIG_OK;}
''' + function(rect, 'void sub_52DC90(float x, float y, TigLine* line, unsigned int* flags)') + '\n' + function(rect, 'int tig_line_intersection(const TigRect* rect, TigLine* line)') + '\n' + function(video, 'int tig_video_buffer_line(TigVideoBuffer* video_buffer, TigLine* line, TigRect* a3, tig_color_t color)') + r'''
static void check(int w,int h,TigLine input,bool via_window) {
    int pitch=w+3, guard=pitch*2, count=pitch*h+2*guard;
    uint32_t* pixels=malloc((size_t)count*sizeof(*pixels));assert(pixels);
    for(int i=0;i<count;i++)pixels[i]=0x12345678;
    Surface s={w,h,pitch*4,pixels+guard};TigVideoBuffer b={&s};
    TigRect bounds={0,0,w,h};TigLine clipped=input;
    if(!via_window || tig_line_intersection(&bounds,&clipped)==TIG_OK)
        assert(tig_video_buffer_line(&b,&clipped,&bounds,0xabcdef)==TIG_OK);
    assert(locks==0);
    for(int i=0;i<guard;i++) {
        assert(pixels[i]==0x12345678);
        assert(pixels[guard+pitch*h+i]==0x12345678);
    }
    for(int y=0;y<h;y++)for(int x=w;x<pitch;x++)assert(pixels[guard+y*pitch+x]==0x12345678);
    free(pixels);
}
int main(void) {
    /* Coordinates recovered from the world-map crash minidump. The bottom
     * endpoint was accepted at height, one row past the image. */
    check(1431,571,(TigLine){711,355,930,571},true);
    for(style=TIG_LINE_STYLE_SOLID;style<=TIG_LINE_STYLE_DASHED;style++)
        for(int x1=-2;x1<=10;x1++)for(int y1=-2;y1<=10;y1++)
        for(int x2=-2;x2<=10;x2++)for(int y2=-2;y2<=10;y2++) {
            check(8,8,(TigLine){x1,y1,x2,y2},true);
            check(8,8,(TigLine){x1,y1,x2,y2},false);
        }
    TigRect r={4,6,8,9};TigLine crossing={0,8,20,8};
    assert(tig_line_intersection(&r,&crossing)==TIG_OK);
    assert(crossing.x1==4 && crossing.x2==11 && crossing.y1==8 && crossing.y2==8);
    r=(TigRect){0,0,0,8};crossing=(TigLine){0,0,1,1};
    assert(tig_line_intersection(&r,&crossing)==TIG_ERR_NO_INTERSECTION);
    r=(TigRect){0,0,1,1};crossing=(TigLine){-1,-1,1,1};
    assert(tig_line_intersection(&r,&crossing)==TIG_OK);
    assert(!crossing.x1 && !crossing.y1 && !crossing.x2 && !crossing.y2);
    puts("PASS: captured route crash, all octants/styles, direct buffer calls, padding, crossing lines, empty and one-pixel clips");
}
'''
with tempfile.TemporaryDirectory() as directory:
    source=Path(directory)/'line.c';exe=Path(directory)/'line'
    source.write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(source),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,timeout=30)
