from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/anim.c').read_text()
start = source.rindex('    if (!critter_is_monstrous(obj)) {', 0, source.index('sound_id = sfx_critter_sound(obj, CRITTER_SOUND_FOOTSTEPS)'))
end = source.index('\n    if ((run_info->path.flags', start)
code = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#define OBJ_HANDLE_NULL 0
#define TIG_OK 0
#define TIG_ART_ANIM_WALK 1
#define TIG_ART_ANIM_RUN 2
#define CRITTER_SOUND_FOOTSTEPS 0
static int calls, distance, current_frame=3;
static int64_t player=1;
static bool monstrous;
static bool critter_is_monstrous(int64_t obj){return monstrous;}
static int tig_art_id_anim_get(int art){return art;}
static int tig_art_anim_data(int art,int *data){return TIG_OK;}
static int tig_art_id_frame_get(int art){return current_frame;}
static int sfx_critter_sound(int64_t obj,int type){return 42;}
static void gsound_play_sfx_on_obj(int sound,int loops,int64_t obj){calls++;}
static int64_t player_get_local_pc_obj(void){return player;}
static int64_t object_dist(int64_t a,int64_t b){assert(b!=OBJ_HANDLE_NULL);return distance;}
static void step(int art_id){int64_t obj=2;int anim,frame,sound_id,art_anim_data;
''' + source[start:end] + r'''
}
int main(void){
    distance=11;step(TIG_ART_ANIM_WALK);assert(calls==0);
    distance=1000;step(TIG_ART_ANIM_RUN);assert(calls==0);
    distance=10;step(TIG_ART_ANIM_WALK);assert(calls==1);
    distance=0;current_frame=8;step(TIG_ART_ANIM_RUN);assert(calls==2);
    current_frame=4;step(TIG_ART_ANIM_RUN);assert(calls==2);
    current_frame=3;step(99);assert(calls==2);
    monstrous=true;step(TIG_ART_ANIM_WALK);assert(calls==2);
    monstrous=false;player=OBJ_HANDLE_NULL;step(TIG_ART_ANIM_WALK);assert(calls==2);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-footsteps-') as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: footsteps limited to 10 tiles; walk/run timing and exclusions preserved')
