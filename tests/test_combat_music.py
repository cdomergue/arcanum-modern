#!/usr/bin/env python3
"""Check immediate player-initiated combat music and scheme restoration."""
from pathlib import Path
import os, subprocess, tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/game/gsound.c').read_text()
def function(name):
 a=s.index('void '+name+'(');return s[a:s.index('\n}',a)+2]
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define TIG_MAX_PATH 260
#define OBJ_HANDLE_NULL 0
#define OBJ_F_TYPE 0
#define OBJ_TYPE_NPC 1
#define OBJ_TYPE_PC 2
#define GSOUND_SCHEME_SLOTS 2
#define GSOUND_SCHEME_FADE_MS 1000
#define GSOUND_COMBAT_MUSIC_TRACKS 3
static bool gsound_combat_music_active;
static int gsound_pre_combat_scheme_idxs[2],gsound_combat_tension_handle,gsound_combat_music_handle;
static struct {int scheme_num,scheme_idx;} gsound_active_schemes[2]={{1,42},{1,17}};
static struct {bool no_combat_music;} compatibility;
#define modern_compat_get() (&compatibility)
static int once,loop,stops,destroyed,restored;
static int obj_field_int32_get(int64_t o,int f){(void)f;return o==1||o==3?OBJ_TYPE_PC:o==2?OBJ_TYPE_NPC:0;}
static bool player_is_local_pc_obj(int64_t o){return o==1;}
static int random_between(int a,int b){(void)b;return a;}
static void gsound_scheme_stop_all(int fade){assert(fade==GSOUND_SCHEME_FADE_MS);stops++;}
static int gsound_play_music_once(const char* path,int fade){assert(strstr(path,"combat 1.mp3") && fade==0);once++;return 10;}
static int gsound_play_music_indefinitely(const char* path,int fade){assert(strstr(path,"combatmusic.mp3") && fade==0);loop++;return 11;}
static void tig_sound_destroy(int handle){assert(handle==10 || handle==11);destroyed++;}
static void gsound_play_scheme(int music,int ambient){assert(music==42 && ambient==17);restored++;}
''' + function('gsound_start_combat_music') + function('gsound_stop_combat_music') + r'''
int main(void){
 assert(player_is_local_pc_obj(1));
 gsound_start_combat_music(1);assert(gsound_combat_music_active && once==1 && loop==1 && stops==1);
 gsound_start_combat_music(1);gsound_start_combat_music(2);assert(once==1 && stops==1);
 gsound_stop_combat_music(2);assert(gsound_combat_music_active);
 gsound_stop_combat_music(1);assert(!gsound_combat_music_active && destroyed==2 && restored==1);
 gsound_start_combat_music(0);gsound_start_combat_music(3);gsound_start_combat_music(4);assert(once==1);
 compatibility.no_combat_music=true;gsound_start_combat_music(1);gsound_start_combat_music(2);assert(once==1);
 compatibility.no_combat_music=false;gsound_start_combat_music(2);assert(once==2 && loop==2);
 gsound_stop_combat_music(1);assert(restored==2);
 puts("PASS: local player starts music immediately, NPC trigger, duplicate suppression, scheme restoration and UAP NoCombatMusic");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-combat-music-') as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
