#!/usr/bin/env python3
"""Check complete HUD message wrapping with the real UTF-8 paragraph renderer."""
from pathlib import Path
import os, subprocess, tempfile
ui=(Path(__file__).resolve().parents[1]/'source/src/ui/modern_ui.c').read_text()
wrap=ui[ui.index('static int menu_paragraph_aligned('):ui.index('\nstatic void description_panel(')]
decode=ui[ui.index('static int codepoint('):ui.index('/* Game strings are Windows-1252;')]
message=ui[ui.index('static bool hud_message_visible('):ui.index('static void draw_hud(void)')]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#define GLYPH_FIRST 32
#define GLYPH_COUNT 352
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {unsigned char r,g,b,a;} SDL_Color;
static struct {struct {float xadvance;} glyphs[GLYPH_COUNT];float density;} fonts[2];
static char drawn[32768];
static float text_measure(int font,const char* s){(void)font;return strlen(s)*8;}
static void text_draw(int font,const char* s,float x,float y,SDL_Color c){
 (void)font;(void)x;(void)y;(void)c;
 strcat(drawn,s);
}

static char message_text[1024];
static uint64_t message_time,now=1;
static float canvas_w=1600,canvas_h=900;
static SDL_Color GOLD,MUTED;
static SDL_FRect box;
static uint64_t SDL_GetTicks(void){return now;}
static void fill(SDL_FRect r,SDL_Color c){(void)c;box=r;}
static void outline(SDL_FRect r,SDL_Color c){(void)r;(void)c;}
''' + decode + wrap + message + r'''

static void compact(const char* src,char* dst){while(*src){if(*src!=' '&&*src!='\n')*dst++=*src;src++;}*dst=0;}
static void check(const char* text){
 strcpy(message_text,text);drawn[0]=0;
 draw_hud_message();
 char actual[32768],expected[32768];compact(drawn,actual);compact(text,expected);
 assert(!strcmp(actual,expected));
 assert(box.x>=0 && box.x+box.w<=canvas_w && box.y>=80);
 assert(box.y+box.h<=canvas_h-164);
 assert(box.h>51);
}
int main(void){
 fonts[1].density=1;for(int i=0;i<GLYPH_COUNT;i++)fonts[1].glyphs[i].xadvance=12;
 strcpy(message_text,"Short message");assert(!hud_message_expanded());
 char long_text[1000]="";
 for(int i=0;i<8;i++)strcat(long_text,"Description complète : forêt, débris et mystérieuse inscription. ");
 strcpy(message_text,long_text);assert(hud_message_expanded());
 for(int w=1200;w<=1600;w+=400){canvas_w=w;check(long_text);}
 now=3999;assert(hud_message_expanded());
 now=4000;assert(!hud_message_expanded() && !hud_message_visible());
 strcpy(message_text,"Short message");assert(hud_message_visible());
 now=12000;assert(!hud_message_visible());
 puts("PASS: complete accented long messages, wrapping above HUD, short messages and expiration");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-hud-message-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
