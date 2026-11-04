"""Exercise the actual tutorial start override independently of rendering."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'source/src/ui/modern_tutorial.inc').read_text(encoding='utf-8')
a=s.index('static void tutorial_start_location(');f=s[a:s.index('\n}\n',a)+2]
h=r'''
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#define MAP_TYPE_START_MAP 0
#define SDL_strcasecmp strcmp
#define SDL_strlcpy(dst,src,n) snprintf(dst,n,"%s",src)
static int64_t x=92958,y=82592;
static char name[256]="Arcanum1-024-fixed";
static int map_by_type(int type){return 1;}
static bool map_get_name(int map,char** out){*out=name;return true;}
static bool map_get_starting_location(int map,int64_t* a,int64_t* b){*a=x;*b=y;return true;}
static bool map_get_worldmap(int map,int* out){*out=0;return true;}
static bool map_get_area(int map,int* out){*out=0;return true;}
static void map_list_info_set(int idx,char* n,int64_t a,int64_t b,bool start,int world,int area){assert(idx==0 && start);x=a;y=b;}
'''
t=r'''
int main(void){
 tutorial_start_location(false);assert(x==92958 && y==82592);
 tutorial_start_location(true);assert(x==25621 && y==50332);
 tutorial_start_location(true);tutorial_start_location(false);assert(x==92958 && y==82592);
 tutorial_start_location(true);tutorial_start_location(false);assert(x==92958 && y==82592);
 strcpy(name,"AnotherModule");tutorial_start_location(true);assert(x==92958 && y==82592);
 puts("Campaign, tutorial, repeated replay, campaign restoration and other-module guard: OK");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(h+f+t)
 subprocess.run(['cc','-std=c11',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
