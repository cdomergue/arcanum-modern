#!/usr/bin/env python3
"""Check dialogue-exit actor selection and bounded persistent diagnostics."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/intgame.c').read_text()
start=s.index('static void intgame_dialog_restore_camera(')
body=s[start:s.index('\n}',start)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
static int64_t selected,center;
static int64_t modern_party_selected(void){return selected;}
static void sub_551A10(int64_t obj){center=obj;}
static int64_t obj_field_int64_get(int64_t obj,int field){(void)field;return obj;}
static void location_calc_dist_from_screen_center(int64_t loc,int64_t* x,int64_t* y){*x=loc-center;*y=0;}
static uint64_t SDL_GetTicks(void){return 12345;}
enum {OBJ_F_LOCATION};
''' +body+r'''
int main(void){
 selected=2;intgame_dialog_restore_camera(1);assert(center==2);
 selected=3;intgame_dialog_restore_camera(1);assert(center==3);
 selected=0;intgame_dialog_restore_camera(1);assert(center==1);
 intgame_dialog_restore_camera(0);assert(center==0);
 FILE* f=fopen("ArcanumModern/dialog-camera.log","wb");assert(f);
 fseek(f,1024*1024-1,SEEK_SET);fputc('x',f);fclose(f);
 selected=2;errno=EDOM;intgame_dialog_restore_camera(1);assert(errno==EDOM);
 assert(!rename("ArcanumModern","hidden"));intgame_dialog_restore_camera(1);assert(center==2);
 assert(!rename("hidden","ArcanumModern"));
 puts("PASS: selected companion, alternate companion, hero fallback, log rotation and unwritable log");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-dialog-camera-') as folder:
 p=Path(folder);(p/'ArcanumModern').mkdir();(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
 assert (p/'ArcanumModern/dialog-camera.previous.log').stat().st_size==1024*1024
 assert 'actor=2 hero=1 selected=2 centered=1' in (p/'ArcanumModern/dialog-camera.log').read_text()
