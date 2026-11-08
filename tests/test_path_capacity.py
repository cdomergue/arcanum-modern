#!/usr/bin/env python3
"""Distant route estimates saturate the real animation buffer without false errors."""
from pathlib import Path
import os, subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/game/anim.c').read_text()
start=s.index('int sub_426320(AnimPath* anim_path, int64_t from, int64_t to, int64_t obj)\n{')
body=s[start:s.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#define ASSERT assert
typedef struct {int maxPathLength,absMaxPathLength,field_CC;} AnimPath;
static int warnings;
static int64_t location_dist(int64_t a,int64_t b){return llabs(a-b);}
static int64_t location_get_x(int64_t a){return a;}
static int64_t location_get_y(int64_t a){return a;}
static void anim_create_path_max_length(int64_t actor,const char* label,int value){(void)actor;(void)label;(void)value;warnings++;}
static void tig_debug_printf(const char* text,...){(void)text;}
''' +body+r'''
int main(void){
 AnimPath p={0,0,200};assert(sub_426320(&p,0,80,1)==200&&!warnings);
 p=(AnimPath){0,0,200};assert(sub_426320(&p,0,200,1)==200&&!warnings);
 p=(AnimPath){0,0,200};assert(sub_426320(&p,0,10,1)==45&&!warnings);
 p=(AnimPath){80,60,200};assert(sub_426320(&p,0,80,1)==60&&!warnings);
 p=(AnimPath){0,0,16};assert(sub_426320(&p,0,10,1)==16&&!warnings);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-path-capacity-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS: distant estimates saturate capacity; short and resumed path budgets preserved')
