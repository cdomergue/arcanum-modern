from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_ui.c').read_text()
start = source.index('static void world_zoom_sync(void)')
end = source.index('\nstatic bool world_zoom_event', start)
code = r'''
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include "game/highres_config.h"
static int zoom_base_width, zoom_base_height, actual_w=800, actual_h=600;
static int output_w=1728, output_h=1084, calls;
static float world_zoom=1, zoom_anchor_x=.5f, zoom_anchor_y=.5f;
static bool zoom_pending, active=true;
static void* window=(void*)1;
static bool SDL_GetWindowSize(void* w,int* x,int* y){*x=output_w;*y=output_h;return true;}
#define SDL_Log(...) ((void)0)
static int hrp_iso_window_width_get(void){return actual_w;}
static int hrp_iso_window_height_get(void){return actual_h;}
static bool world_zoom_allowed(void){return active;}
static bool modern_world_resize(int w,int h,float x,float y){
    assert(w>=800 && h>=600 && w<=4096 && h<=4096);
    actual_w=w;actual_h=h;calls++;return true;
}
''' + source[start:end] + r'''
int main(void){
    FILE* f=fopen("modern-ui/graphics.ini","w");
    fputs("Mode=0\nWidth=1728\nHeight=1084\n",f);fclose(f);
    highres_config_load();
    const HighResConfig* c=highres_config_get();
    assert(c->loaded && c->width==1728 && c->height==1084 && c->windowed);
    f=fopen("HighRes/config.ini","w");fputs("Width=800\nHeight=600\n",f);fclose(f);
    highres_config_load();assert(c->width==1728 && c->height==1084);
    remove("modern-ui/graphics.ini");highres_config_load();assert(!c->loaded);
    world_zoom_sync();assert(actual_w==1728 && actual_h==1084);
    world_zoom_sync();assert(calls==1);
    world_zoom=.8f;world_zoom_sync();assert(actual_w==2160 && actual_h==1355);
    output_w=1920;output_h=1080;world_zoom_sync();assert(actual_w==2400 && actual_h==1350);
    active=false;world_zoom_sync();assert(actual_w==1920 && actual_h==1080);
    output_w=800;output_h=500;world_zoom_sync();assert(actual_w==960 && actual_h==600);
    output_w=0;output_h=0;world_zoom_sync();assert(actual_w==960 && actual_h==600);
    output_w=7680;output_h=4320;active=true;world_zoom=.5f;world_zoom_sync();
    assert(actual_w==4096 && actual_h==2304);
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-display-') as directory:
    work = Path(directory)
    (work / 'modern-ui').mkdir()
    (work / 'HighRes').mkdir()
    (work / 'SDL3').mkdir()
    (work / 'SDL3/SDL_stdinc.h').write_text('#include <strings.h>\n#define SDL_strcasecmp strcasecmp\n')
    (work / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-I', str(work),
                    '-I', str(root / 'source/src'), str(work / 'test.c'),
                    str(root / 'source/src/game/highres_config.c'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test')], cwd=work, check=True)
print('PASS: graphics.ini alone, HighRes ignored, automatic aspect ratio, resize and zoom limits')
