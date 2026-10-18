#!/usr/bin/env python3
"""Check paragraph boundaries across pages without losing or repeating lines."""
from pathlib import Path
import os, subprocess, tempfile

source=(Path(__file__).resolve().parents[1]/'source/src/ui/modern_panels.inc').read_text()
start=source.index('static int written_book_page(')
function=source[start:source.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
typedef struct {float x,y,w,h;} SDL_FRect;
#define INK 0
static int seen[256];
static const char* paragraphs[]={"A","BCDEF","GHIJ","KL","M"};
static const char* modern_written_book_paragraph(int i,bool* center,int* font){
    *center=i==0;*font=i==4?744:37;return i<5?paragraphs[i]:NULL;
}
static const char* game_text(const char* s){return s;}
/* One test character represents a wrapped production line. */
static int menu_paragraph(const char* s,SDL_FRect r,int offset,int color){
    (void)color;int rows=strlen(s),count=r.h/27;
    for(int i=offset;i<rows && i<offset+count;i++)seen[(unsigned char)s[i]]++;
    return rows;
}
static void centered(int font,const char* s,float x,float y,int color){
    (void)font;(void)x;(void)y;(void)color;assert(strlen(s)==1);seen[(unsigned char)*s]++;
}
static void written_book_picture(const char* s,SDL_FRect r,int offset){
    (void)s;menu_paragraph("MNOPQR",r,offset,INK);
}
'''+function+r'''
int main(void){
    for(int page_rows=1;page_rows<=7;page_rows++){
        memset(seen,0,sizeof(seen));
        int total=written_book_page((SDL_FRect){0,0,400,0},0);
        assert(total==22);
        for(int offset=0;offset<total;offset+=page_rows){
            assert(written_book_page((SDL_FRect){0,0,400,page_rows*27},offset)==total);
        }
        for(int c='A';c<='R';c++)assert(seen[c]==1);
    }
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-books-') as folder:
    p=Path(folder);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
print('PASS: complete text, centered title, paragraph breaks and page sizes 1–7')
