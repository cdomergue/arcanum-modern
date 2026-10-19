from pathlib import Path
import os
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/game/gsound.c').read_text()
def extract(name):
    match = re.search(r'^void ' + name + r'\([^;\n]*\)\n\{', source, re.M)
    return source[match.start():source.index('\n}', match.end()) + 2]

code = r'''
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define GSOUND_VOLUME_MAX 127
#define GSOUND_BALANCE_CENTER 64
#define GSOUND_ISOMETRIC_Y_SCALE 2
#define OBJ_HANDLE_NULL 0
#define OBJ_F_LOCATION 0
#define GSOUND_PING_INTERVAL_MS 250
typedef int TigSoundPositionalSize;
typedef int tig_timestamp_t;
static int64_t gsound_listener_x,gsound_listener_y,gsound_origin_x,gsound_origin_y,gsound_origin_loc=1;
static bool gsound_initialized=true;
static int64_t player=1, player_location=200;
static int sound_minimum_radius[4]={0,0,0,0};
static int sound_maximum_radius[4]={10000,10000,10000,10000};
static int sound_minimum_pan_distance=150,sound_maximum_pan_distance=400;
static int recalculations, updates;
static int64_t player_get_local_pc_obj(void){return player;}
static int64_t obj_field_int64_get(int64_t obj,int field){assert(obj==player);return player_location;}
static void location_xy(int64_t loc,int64_t* x,int64_t* y){*x=loc*40;*y=loc*20;}
static void set_listener_xy(int64_t x,int64_t y){gsound_listener_x=x;gsound_listener_y=y;recalculations++;}
static void gsound_update(void){updates++;}
static void gsound_listener_set(int64_t loc);
'''
code += extract('gsound_calc_positional_params')
code += '\n' + extract('gsound_listener_set')
code += '\n' + extract('gsound_ping')
code += r'''
int main(void){
    int volume,pan;
    for(int size=0;size<4;size++){
        gsound_calc_positional_params(0,0,&volume,&pan,size);assert(volume>0);
        gsound_calc_positional_params(400,200,&volume,&pan,size);assert(volume>0);
        gsound_calc_positional_params(440,220,&volume,&pan,size);assert(volume==0);
        gsound_calc_positional_params(-440,-220,&volume,&pan,size);assert(volume==0);
        gsound_calc_positional_params(0,440,&volume,&pan,size);assert(volume==0);
        gsound_calc_positional_params(880,0,&volume,&pan,size);assert(volume==0);
    }
    gsound_listener_set(999);
    assert(gsound_listener_x==7960 && gsound_listener_y==3980);
    player_location=201;int before=recalculations;gsound_ping(251);
    assert(recalculations>before && gsound_listener_x==8000 && updates==1);
    player=OBJ_HANDLE_NULL;gsound_listener_set(100);
    assert(gsound_listener_x==3960);
    gsound_calc_positional_params(4840,gsound_listener_y,&volume,&pan,0);assert(volume>0);
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-world-sound-') as directory:
    c = Path(directory) / 'test.c'
    exe = Path(directory) / 'test'
    c.write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', str(c), '-lm', '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: all positional sizes, 10-tile boundary, player listener, ongoing refresh, menu fallback')
