#!/usr/bin/env python3
"""Quickslot labels use actual objects, identified names and localized actions."""
from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_ui.c').read_text()
a=s.index('static void quickslot_hint(');body=s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define SPELL_COUNT 80
enum {HOTKEY_NONE,HOTKEY_ITEM,HOTKEY_SKILL,HOTKEY_SPELL,HOTKEY_ITEM_SPELL};
typedef struct {int type,data;struct {int64_t obj;} item_obj;} Hotkey;
static Hotkey slots[10];
static Hotkey* sub_57F240(int n){return &slots[n];}
static bool obj_handle_is_valid(int64_t o){return o==1||o==2;}
static int64_t player_get_local_pc_obj(void){return 42;}
static void object_examine(int64_t o,int64_t pc,char* name){assert(pc==42);strcpy(name,o==1?"Potion identifiée":"Potion inconnue");}
static const char* spell_name(int n){assert(n>=0&&n<80);return "Soin léger";}
static const char* sub_57A700(int n){assert(n>=0&&n<4);return "Discrétion";}
static const char* game_text(const char* s){return s;}
static void SDL_strlcpy(char* d,const char* s,size_t n){snprintf(d,n,"%s",s);}
static void SDL_strlcat(char* d,const char* s,size_t n){size_t k=strlen(d);if(k<n)snprintf(d+k,n-k,"%s",s);}
'''+body+r'''
int main(void){
 char hint[4096];quickslot_hint(-1,hint,sizeof(hint));assert(!*hint);
 quickslot_hint(0,hint,sizeof(hint));assert(!*hint);
 slots[0]=(Hotkey){.type=HOTKEY_ITEM,.item_obj={1}};quickslot_hint(0,hint,sizeof(hint));assert(!strcmp(hint,"[1] Potion identifiée"));
 slots[0].item_obj.obj=2;quickslot_hint(0,hint,sizeof(hint));assert(strstr(hint,"inconnue"));
 slots[0].item_obj.obj=3;quickslot_hint(0,hint,sizeof(hint));assert(!*hint);
 slots[9]=(Hotkey){.type=HOTKEY_SPELL,.data=2};quickslot_hint(9,hint,sizeof(hint));assert(!strcmp(hint,"[0] Soin léger"));
 slots[9].data=80;quickslot_hint(9,hint,sizeof(hint));assert(!*hint);
 slots[9]=(Hotkey){.type=HOTKEY_SKILL,.data=0};quickslot_hint(9,hint,sizeof(hint));assert(strstr(hint,"Discrétion"));
 slots[9]=(Hotkey){.type=HOTKEY_ITEM_SPELL,.data=2,.item_obj={1}};quickslot_hint(9,hint,sizeof(hint));assert(strstr(hint,"Potion identifiée")&&strstr(hint,"Soin léger"));
 puts("PASS: empty/stale slots, identified objects, localized spell/skill names, item-spells and digit 0");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-hints-') as folder:
 p=Path(folder);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
