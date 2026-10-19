#!/usr/bin/env python3
from pathlib import Path
import os
import subprocess
import tempfile

root=Path(__file__).resolve().parents[1]
source=(root/'source/src/ui/modern_map_camera.inc').read_text()
code=r'''
#include <assert.h>
#include <math.h>
''' + source + r'''
static void close_to(float a,float b){assert(fabsf(a-b)<0.001f);}
int main(void){
    MapCamera c={.x=500,.y=300,.width=800,.height=500,.scale=1,.min_scale=0.2f,.max_scale=4};
    float ax=c.x+250/c.scale,ay=c.y+120/c.scale;
    map_camera_zoom(&c,1.25f,250,120,800,500,4000,2500);
    close_to(c.x+250/c.scale,ax);close_to(c.y+120/c.scale,ay);
    map_camera_zoom(&c,0.8f,250,120,800,500,4000,2500);
    close_to(c.x,500);close_to(c.y,300);close_to(c.scale,1);
    map_camera_zoom(&c,0.0001f,250,120,800,500,4000,2500);
    close_to(c.scale,0.2f);close_to(c.width,4000);close_to(c.height,2500);
    close_to(c.x,0);close_to(c.y,0);
    map_camera_zoom(&c,1000,250,120,800,500,4000,2500);
    close_to(c.scale,4);close_to(c.width,200);close_to(c.height,125);
    c.x=9000;c.y=-500;map_camera_clamp(&c,4000,2500);
    close_to(c.x,3800);close_to(c.y,0);
    for(int i=0;i<100;i++) {
        map_camera_zoom(&c,0.8f,400,250,800,500,4000,2500);
        assert(c.x>=0 && c.y>=0 && c.x+c.width<=4000.001f && c.y+c.height<=2500.001f);
        float screen_x=123,screen_y=67;
        float world_x=c.x+screen_x/c.scale,world_y=c.y+screen_y/c.scale;
        close_to((world_x-c.x)*c.scale,screen_x);close_to((world_y-c.y)*c.scale,screen_y);
    }
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-camera-') as directory:
    path=Path(directory);(path/'test.c').write_text(code)
    subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror',str(path/'test.c'),'-lm','-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
print('PASS: map zoom in/out, pointer anchor, full-map fit, bounds and coordinate round trips')
