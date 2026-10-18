#!/usr/bin/env python3
"""Execute production spell quotes and sound loading without mutating a game."""
from pathlib import Path
import os, re, subprocess, tempfile

root = Path(__file__).resolve().parents[1] / 'source/src'
def extract(path, name):
    source = (root / path).read_text()
    m = re.search(r'^\w[^\n]*\b' + name + r'\([^;\n]*\)\n\{', source, re.M)
    assert m, name
    return source[m.start():source.index('\n}', m.end()) + 2]

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#define SPELL_COUNT 80
#define COLLEGE_FROM_SPELL(s) ((s)/5)
#define STAT_RACE 1
#define RACE_DWARF 1
#define OBJ_F_CRITTER_FLAGS 1
#define OBJ_F_ITEM_MANA_STORE 2
#define OCF_FATIGUE_LIMITING 1
#define FIRST_WEAR_INV_LOC 0
#define LAST_WEAR_INV_LOC 1
static struct {int cost;} magictech_spells[80];
static bool magictech_cheat_mode,known=true;
static int mastery=-1,race,flags,fatigue=30,mana[2];
static bool spell_is_known(int64_t a,int s){(void)a;(void)s;return known;}
static int spell_mastery_get(int64_t a){(void)a;return mastery;}
static int stat_level_get(int64_t a,int s){(void)a;(void)s;return race;}
static int obj_field_int32_get(int64_t a,int f){return f==OBJ_F_CRITTER_FLAGS?flags:mana[a-2];}
static int64_t item_wield_get(int64_t a,int s){(void)a;return 2+s;}
static int critter_fatigue_current(int64_t a){(void)a;return fatigue;}
#define GSOUND_SCHEME_SLOTS 2
typedef struct {int* stream;} GameLoadInfo;
static int gsound_pending_scheme_restore,gsound_post_song_scheme_idxs[2];
static int gsound_combat_music_active,gsound_pre_combat_scheme_idxs[2];
static int gsound_combat_music_handle,position,loops,schemes;
static int tig_file_fread(void* dst,size_t size,size_t n,int* stream){
    assert(size==sizeof(int)&&n==1);memcpy(dst,&stream[position++],size);return 1;
}
static int gsound_play_music_indefinitely(const char* path,int fade){
    assert(!strcmp(path,"sound\\music\\combatmusic.mp3")&&fade==0);loops++;return 42;
}
static void gsound_play_scheme(int a,int b){assert(a==7&&b==8);schemes++;}
'''
code += extract('game/magictech.c', 'modern_spell_resources')
code += '\n' + extract('game/gsound.c', 'gsound_load')
code += r'''
int main(void){
 int cost;magictech_spells[0].cost=5;
 assert(modern_spell_resources(1,0,&cost)&&cost==5);
 known=false;assert(!modern_spell_resources(1,0,&cost));known=true;
 assert(!modern_spell_resources(0,0,&cost));
 assert(!modern_spell_resources(1,80,&cost));
 fatigue=-10;assert(!modern_spell_resources(1,0,&cost));
 fatigue=-9;assert(modern_spell_resources(1,0,&cost));
 fatigue=30;mastery=0;magictech_spells[0].cost=7;
 assert(modern_spell_resources(1,0,&cost)&&cost==4); /* original odd rounding */
 race=RACE_DWARF;assert(modern_spell_resources(1,0,&cost)&&cost==8);
 flags=OCF_FATIGUE_LIMITING;assert(modern_spell_resources(1,0,&cost)&&cost==4);
 mana[0]=3;mana[1]=2;
 for(int i=0;i<100;i++)assert(modern_spell_resources(1,0,&cost)&&cost==0);
 assert(mana[0]==3&&mana[1]==2&&fatigue==30); /* hovering spends nothing */
 mana[0]=mana[1]=0;race=flags=0;mastery=-1;magictech_cheat_mode=true;
 assert(modern_spell_resources(1,0,&cost)&&cost==1);
 int data[]={7,8,0,0,0,1,12,13};GameLoadInfo load={data};
 assert(gsound_load(&load)&&loops==1&&schemes==0&&gsound_combat_music_handle==42);
 assert(gsound_pre_combat_scheme_idxs[0]==12&&gsound_pre_combat_scheme_idxs[1]==13);
 position=0;data[5]=0;assert(gsound_load(&load)&&loops==1&&schemes==1);
 puts("PASS: spell availability, fatigue/mastery/race/mana quotes, read-only resources and saved combat music/ambience");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-resources-') as directory:
    p = Path(directory)
    (p/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'), '-std=c17', '-Wall', '-Wextra', str(p/'test.c'), '-o', str(p/'test')], check=True)
    subprocess.run([str(p/'test')], check=True)
