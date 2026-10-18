#!/usr/bin/env python3
"""Verify native option changes use original callbacks, bounds and MES values."""
from pathlib import Path
import os
import re
import subprocess
import tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/ui/cyclic_ui.c').read_text()
def function(name):
 m=re.search(r'^(?:void|bool) '+name+r'\([^\n]*\)\n\{',s,re.M);a=m.start();b=m.end();depth=1
 while depth:
  depth+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
enum {CYCLIC_UI_CONTROL_NUMERIC_BAR,CYCLIC_UI_CONTROL_MESSAGE_FILE,CYCLIC_UI_CONTROL_TEXT_ARRAY};
typedef struct {const char* label;const char* text;int value,maximum;bool enabled,numeric;} ModernOption;
typedef struct {int num;const char* str;} MesFileEntry;
typedef struct {struct {const char* title;bool enabled;int type;const char* const* text_array;int text_array_size;void(*value_changed_callback)(int);} info;int value,max_value,mes_file;} CyclicUiControl;
static CyclicUiControl control;static int callbacks,last_value;
static bool get_control(CyclicUiControl** p,int id,const char* f){(void)f;if(id!=1)return false;*p=&control;return true;}
static bool mes_search(int file,MesFileEntry* e){assert(file==9);e->str=e->num?"Oui":"Non";return true;}
static void cyclic_ui_refresh_level(CyclicUiControl* c){(void)c;}
static void changed(int value){callbacks++;last_value=value;}
'''+'\n'.join(function(n) for n in ['cycle_left','cycle_right','modern_cyclic_state','modern_cyclic_adjust'])+r'''
int main(void){
 ModernOption o;
 control.info.title="Original label";control.info.enabled=true;control.info.value_changed_callback=changed;
 control.info.type=CYCLIC_UI_CONTROL_MESSAGE_FILE;control.max_value=1;control.mes_file=9;
 assert(modern_cyclic_state(1,&o)&&strcmp(o.label,"Original label")==0&&strcmp(o.text,"Non")==0);
 modern_cyclic_adjust(1,1);assert(last_value==1&&callbacks==1);
 assert(modern_cyclic_state(1,&o)&&strcmp(o.text,"Oui")==0);
 modern_cyclic_adjust(1,1);assert(last_value==0&&callbacks==2);
 modern_cyclic_adjust(1,-1);assert(last_value==1&&callbacks==3);
 control.info.enabled=false;modern_cyclic_adjust(1,1);assert(callbacks==3);
 control.info.enabled=true;control.info.type=CYCLIC_UI_CONTROL_NUMERIC_BAR;control.max_value=10;control.value=10;
 modern_cyclic_adjust(1,1);assert(callbacks==3&&control.value==10);
 modern_cyclic_adjust(1,-1);assert(callbacks==4&&last_value==9);
 control.value=0;modern_cyclic_adjust(1,-1);assert(callbacks==4&&control.value==0);
 assert(modern_cyclic_state(1,&o)&&o.numeric&&o.maximum==10);
 const char* modules[]={"Arcanum","Module 2"};control.info.type=CYCLIC_UI_CONTROL_TEXT_ARRAY;
 control.info.text_array=modules;control.info.text_array_size=2;control.max_value=1;control.value=1;
 assert(modern_cyclic_state(1,&o)&&strcmp(o.text,"Module 2")==0);
 assert(!modern_cyclic_state(-1,&o));modern_cyclic_adjust(-1,1);assert(callbacks==4);
 puts("PASS: original localized values, setting callbacks, enum wrapping, numeric bounds and disabled controls");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-options-test-') as d:
 c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
