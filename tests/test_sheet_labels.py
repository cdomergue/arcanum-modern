#!/usr/bin/env python3
"""Localized sheet tabs retain full UTF-8 and fit at each button text scale."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_sheet.inc').read_text()
start = source.index('static void sheet_tab(')
sheet = source[start:source.index('\n}', start) + 2]
source = (root / 'source/src/ui/modern_ui.c').read_text()
start = source.index('static int codepoint(')
decoder = source[start:source.index('\n}', start) + 2]
source = (root / 'source/src/ui/modern_button_text.inc').read_text()
layout = source[source.index('typedef struct ButtonTextLayout'):source.index('static void button_label(')]
code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define GLYPH_FIRST 32
#define GLYPH_COUNT 352
#define BUTTON_FONT 0
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {unsigned char r,g,b,a;} SDL_Color;
typedef struct {int x0,y0,x1,y1;float xadvance,xoff,xoff2,yoff,yoff2;} stbtt_packedchar;
typedef struct {stbtt_packedchar glyphs[GLYPH_COUNT];float size,density;} NativeFont;
static NativeFont fonts[1];
static size_t SDL_strlcpy(char* dst,const char* src,size_t size)
{size_t n=strlen(src);if(size){size_t copy=n<size-1?n:size-1;memcpy(dst,src,copy);dst[copy]=0;}return n;}
static size_t SDL_strlcat(char* dst,const char* src,size_t size)
{size_t n=strlen(dst);return n+SDL_strlcpy(dst+n,src,size-n);}
''' + decoder + '\n' + layout + r'''
static int button_action;
static bool button_active;
static ButtonTextLayout last;
static void button(SDL_FRect rect,int action,const char* label,int icon,bool active,bool available)
{(void)rect;(void)label;(void)icon;assert(available);button_action=action;button_active=active;}
static SDL_Color menu_control_color(SDL_FRect rect,bool active,bool available)
{(void)rect;(void)active;(void)available;return (SDL_Color){0};}
static void button_label(int font,const char* text,SDL_FRect rect,bool center,SDL_Color color)
{
 (void)color;assert(center&&font==BUTTON_FONT);
 assert(rect.x>=20&&rect.y>=30&&rect.x+rect.w<=145&&rect.y+rect.h<=72);
 last=button_text_layout(font,text,rect.w,rect.h);
 assert(last.height<=rect.h);
 for(int i=0;i<last.count;i++)assert(last.ink[i].w<=rect.w);
}
''' + sheet + r'''
static void font_size(int scale)
{
 fonts[0].size=14*scale/100.f;fonts[0].density=1;
 for(int i=0;i<GLYPH_COUNT;i++)fonts[0].glyphs[i]=(stbtt_packedchar){0,0,8,10,7.5f*scale/100.f,0,7.5f*scale/100.f,-10*scale/100.f,0};
}
int main(void)
{
 for(int scale=75;scale<=150;scale+=25){
  font_size(scale);
  sheet_tab((SDL_FRect){20,30,125,42},77,"Compétences technologiques",true);
  assert(button_action==77&&button_active&&last.count==2);
  if(scale<=100)assert(!last.shortened&&!strcmp(last.lines[0],"Compétences")&&!strcmp(last.lines[1],"technologiques"));
  else assert(last.shortened);
  sheet_tab((SDL_FRect){20,30,125,42},78,"Technological Skills",false);
  assert(button_action==78&&!button_active&&last.count==2);
  sheet_tab((SDL_FRect){20,30,125,42},79,"Arcane",false);
  assert(last.count==1&&!last.shortened&&!strcmp(last.lines[0],"Arcane"));
  ButtonTextLayout l=button_text_layout(0,"ÉéÉéÉéÉéÉéÉéÉéÉéÉéÉéÉé",65,36);
  assert(l.shortened&&l.count==1&&strstr(l.lines[0],"...")&&l.ink[0].w<=65);
  const unsigned char* p=(const unsigned char*)l.lines[0];
  while(*p==0xc3){assert(p[1]==0x89||p[1]==0xa9);p+=2;}assert(!strcmp((const char*)p,"..."));
  l=button_text_layout(0,"Long word",1,36);assert(l.shortened&&l.ink[0].w<=1);
 }
 puts("PASS: French/English tabs, 75–150% sizing, bounded lines and UTF-8 truncation");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-sheet-labels-') as folder:
    path = Path(folder)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-Wall', '-Wextra', '-Werror',
                    str(path / 'test.c'), '-lm', '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
