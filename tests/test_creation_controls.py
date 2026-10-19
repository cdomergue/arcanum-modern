#!/usr/bin/env python3
"""Exercise real creation adapters and translated-text wrapping with engine doubles."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
ui = (root / 'source/src/ui/modern_ui.c').read_text()
engine = (root / 'source/src/ui/charedit_ui.c').read_text()
adapter = engine[engine.index('void modern_charedit_adjust('):engine.index('int64_t modern_charedit_object(')]
wrap = ui[ui.index('static int menu_paragraph_aligned('):ui.index('\nstatic void description_panel(')]
decode = ui[ui.index('static int codepoint('):ui.index('/* Game strings are Windows-1252;')]
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GLYPH_FIRST 32
#define GLYPH_COUNT 352
#define SKILL_COUNT 16
#define TECH_COUNT 8
#define COLLEGE_COUNT 16
#define DEGREE_COUNT 8
#define TIG_MESSAGE_BUTTON 1
#define TIG_BUTTON_STATE_PRESSED 2
typedef struct {int type;struct {struct {int state,button_handle;} button;} data;} TigMessage;
typedef struct {int button_handle;} Button;
static Button stru_5C7F88[10],stru_5C8028[10],charedit_skills_plus_buttons[16],charedit_skills_minus_buttons[16];
static int64_t charedit_obj=1;
static int degree,level,points=5,accepted[16];
#define dword_64D364 accepted
static bool charedit_is_created(void){return true;}
static int64_t player_get_local_pc_obj(void){return 1;}
static void charedit_window_message_filter(TigMessage* m){(void)m;}
static void charedit_skills_win_message_filter(TigMessage* m){(void)m;}
static int tech_degree_get(int64_t o,int i){(void)o;assert(i>=0&&i<8);return degree;}
static void tech_ui_inc_degree(int64_t o,int i){(void)o;(void)i;degree++;points--;}
static void tech_ui_dec_degree(int64_t o,int i){(void)o;(void)i;degree--;points++;}
static int spell_college_level_get(int64_t o,int i){(void)o;assert(i>=0&&i<16);return level;}
static void spell_ui_add(int64_t o,int i){(void)o;assert(i==3*5+level);level++;points--;}
static void spell_ui_remove(int64_t o,int i){(void)o;assert(i==3*5+level-1);level--;points++;}
static void charedit_refresh(void){}
typedef struct {float x,y,w,h;} SDL_FRect;
typedef struct {unsigned char r,g,b,a;} SDL_Color;
static struct {struct {float xadvance;} glyphs[GLYPH_COUNT];float density;} fonts[2];
static char drawn[32768];
static float text_measure(int font,const char* s){(void)font;return strlen(s);}
static void text_draw(int font,const char* s,float x,float y,SDL_Color c){
 (void)font;(void)x;(void)y;(void)c;
 strcat(drawn,s);
}
''' + adapter + decode + wrap + r'''
static void compact(const char* src,char* dst){while(*src){if(*src!=' '&&*src!='\t'&&*src!='\n'&&*src!='\r')*dst++=*src;src++;}*dst=0;}
static void check_wrap(const char* text,float width){
 SDL_FRect box={0,0,width,54};SDL_Color color={0};
 char all[32768]="",actual[32768],expected[32768];
 drawn[0]=0;int total=menu_paragraph(text,box,0,color);
 for(int offset=0;offset<total;offset+=2){drawn[0]=0;assert(menu_paragraph(text,box,offset,color)==total);strcat(all,drawn);}
 compact(text,expected);compact(all,actual);assert(strcmp(expected,actual)==0);
}
int main(void){
 modern_charedit_adjust(2,0,1);assert(degree==1&&points==4);
 modern_charedit_adjust(2,0,-1);assert(degree==0&&points==5);
 modern_charedit_adjust(2,0,-1);assert(degree==0&&points==5);
 degree=7;modern_charedit_adjust(2,0,1);assert(degree==7&&points==5);
 modern_charedit_adjust(3,3,1);assert(level==1&&points==4);
 modern_charedit_adjust(3,3,-1);assert(level==0&&points==5);
 modern_charedit_adjust(3,3,-1);assert(level==0&&points==5);
 level=2;accepted[3]=2;modern_charedit_adjust(3,3,-1);assert(level==2&&points==5);
 level=5;modern_charedit_adjust(3,3,1);assert(level==5&&points==5);
 modern_charedit_adjust(2,-1,1);modern_charedit_adjust(3,16,1);assert(points==5);
 fonts[1].density=1;for(int i=0;i<GLYPH_COUNT;i++)fonts[1].glyphs[i].xadvance=1;
 check_wrap("Antécédents : été, forêt, élève.\n\nEine längere Beschreibung.\nSkills and spells.",12);
 check_wrap("éàüéàüéàüéàü",1);
 char long_word[8001];memset(long_word,'x',8000);long_word[8000]=0;
 check_wrap(long_word,3000);
 puts("PASS: point refunds, boundaries, accepted spells and lossless translated-text scrolling");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-creation-test-') as temp:
    source = Path(temp) / 'test.c'
    exe = Path(temp) / 'test'
    source.write_text(harness)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra',
                    str(source), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
