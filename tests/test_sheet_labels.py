#!/usr/bin/env python3
"""Keep localized character-sheet tabs inside their button bounds."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_sheet.inc').read_text()
start = source.index('static void sheet_tab(')
body = source[start:source.index('\n}', start) + 2]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {unsigned char r,g,b,a;} SDL_Color;
static int calls,button_action;
static bool button_active;
static char lines[2][128];
static SDL_FRect bounds[2];
static void button(SDL_FRect rect,int action,const char* label,int icon,bool active,bool available)
{(void)rect;(void)label;(void)icon;assert(available);button_action=action;button_active=active;}
static float text_measure(int font,const char* text){(void)font;return strlen(text)*8;}
static SDL_Color menu_control_color(SDL_FRect rect,bool active,bool available)
{(void)rect;(void)active;(void)available;return (SDL_Color){0};}
static size_t SDL_strlcpy(char* dst,const char* src,size_t size)
{size_t n=strlen(src);if(size){size_t copy=n<size-1?n:size-1;memcpy(dst,src,copy);dst[copy]=0;}return n;}
static void text_compact(const char* text,float x,float y,float width,bool center,SDL_Color color)
{(void)center;(void)color;assert(calls<2);strcpy(lines[calls],text);bounds[calls++]=(SDL_FRect){x,y,width,0};}
''' + body + r'''
static void check_two_lines(const char* label,const char* first,const char* second)
{
 calls=0;sheet_tab((SDL_FRect){20,30,125,42},77,label,true);
 assert(button_action==77&&button_active&&calls==2);
 assert(!strcmp(lines[0],first)&&!strcmp(lines[1],second));
 for(int i=0;i<2;i++)assert(bounds[i].x==30&&bounds[i].w==105);
 assert(bounds[0].y==37&&bounds[1].y==53);
}
int main(void)
{
 check_two_lines("Compétences technologiques","Compétences","technologiques");
 check_two_lines("Technological Skills","Technological","Skills");
 calls=0;sheet_tab((SDL_FRect){20,30,125,42},78,"Arcane",false);
 assert(button_action==78&&!button_active&&calls==1&&!strcmp(lines[0],"Arcane"));
 assert(bounds[0].x==30&&bounds[0].y==45&&bounds[0].w==105);
 puts("PASS: French and English sheet tabs wrap within their button bounds");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-sheet-labels-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
