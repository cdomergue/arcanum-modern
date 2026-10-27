#!/usr/bin/env python3
"""Passive combat inspection must not depend on attack filters or input readiness."""
from pathlib import Path
import os,re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
header=(root/'source/src/ui/modern_ui.h').read_text()
source=(root/'source/src/ui/modern_combat_hover.inc').read_text()
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint64_t Uint64;
#define MAX_STRING 2000
#define OBJ_HANDLE_NULL 0
enum {TIG_OK,OBJ_F_TYPE,INTGAME_MODE_DIALOG=9,TGT_OBJECT=16,TGT_OBJ_NO_T_WALL=32};
typedef struct {int x,y;} TigMouseState;
typedef struct {bool is_loc;int64_t obj;} TargetDescriptor;
''' + re.search(r'typedef struct ModernDetails \{.*?\} ModernDetails;',header,re.S)[0] + r'''
static ModernDetails* modern_details_capture;
static bool modern_details_passthrough,intgame_fullscreen=true;
static ModernDetails modern_hover_snapshot;
static int64_t modern_hover_object,picked=2;
static int mode,formats,lookups,health=90,kind=1;
static bool clearing,world=true,valid=true;
static Uint64 ticks;
static Uint64 SDL_GetTicks(void){return ticks;}
static bool map_is_clearing_objects(void){return clearing;}
static int intgame_mode_get(void){return mode;}
static int tig_mouse_get_state(TigMouseState* m){*m=(TigMouseState){400,300};return TIG_OK;}
static bool sub_5518C0(int x,int y){assert(x==400&&y==300);return world;}
static bool target_pick_at_screen_xy_ex(int x,int y,TargetDescriptor* t,int flags,bool fullscreen){
    assert(x==400&&y==300&&fullscreen);assert(flags==(TGT_OBJECT|TGT_OBJ_NO_T_WALL));
    *t=(TargetDescriptor){.obj=picked};lookups++;return picked!=0;
}
static bool obj_handle_is_valid(int64_t o){(void)o;return valid;}
static int obj_field_int32_get(int64_t o,int f){(void)o;(void)f;return kind;}
static bool obj_type_is_critter(int type){return type==1;}
static void object_examine(int64_t o,int64_t viewer,char* name){assert(viewer==1);snprintf(name,MAX_STRING,"Character %lld",(long long)o);}
static void intgame_examine_critter(int64_t viewer,int64_t o,char* name){
    assert(viewer==1&&o==picked&&modern_details_capture&&!modern_details_passthrough);formats++;
    ModernDetails* d=modern_details_capture;d->portrait=7;d->count=1;
    snprintf(d->lines[0],2000,"%s health %d",name,health);
}
''' + source + r'''
int main(void){
    /* No idle event, attack target or controllable turn is required. */
    const ModernDetails* d=modern_combat_hover_details(1);
    assert(d && d->portrait==7 && !strcmp(d->title,"Character 2"));
    assert(formats==1 && !modern_details_capture && !modern_details_passthrough);
    for(int i=0;i<100;i++)assert(modern_combat_hover_details(1)==d);
    assert(formats==1); /* No repeated scripts or formatting every frame. */
    health=70;ticks=100;d=modern_combat_hover_details(1);assert(strstr(d->lines[0],"70")&&formats==2);
    picked=3;d=modern_combat_hover_details(1);assert(!strcmp(d->title,"Character 3")&&formats==3);
    picked=1;assert(modern_combat_hover_details(1)); /* Self is inspectable too. */
    picked=0;assert(!modern_combat_hover_details(1));
    picked=2;assert(modern_combat_hover_details(1));
    valid=false;assert(!modern_combat_hover_details(1));valid=true;
    world=false;assert(!modern_combat_hover_details(1));world=true;
    clearing=true;assert(!modern_combat_hover_details(1));clearing=false;
    mode=INTGAME_MODE_DIALOG;assert(!modern_combat_hover_details(1));mode=0;
    modern_details_capture=&modern_hover_snapshot;assert(!modern_combat_hover_details(1));modern_details_capture=NULL;
    kind=0;assert(!modern_combat_hover_details(1));
    modern_hover_object=picked;modern_hover_snapshot.count=1;
    assert(modern_combat_hover_details(1)==&modern_hover_snapshot);
    picked=4;assert(!modern_combat_hover_details(1));
    puts("PASS: immediate combat inspection, allies/self, live health, no stale targets, no scripts or attack-filter changes");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-combat-hover-') as directory:
    p=Path(directory);(p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
