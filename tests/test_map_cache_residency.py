from pathlib import Path
import os, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'source/src/ui/modern_map_cache.inc').read_text()
start=s.index('static SDL_Texture* map_cache_tile(')
end=s.index('\nstatic SDL_Texture* map_overlay_texture',start)
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
typedef uint64_t Uint64;
typedef struct {int value;} SDL_Texture;
typedef struct {int value;} SDL_Surface;
typedef struct {int value;} TownMapInfo;
#define SDL_SCALEMODE_LINEAR 1
static void* renderer;
static int loads,live,peak;
static SDL_Surface* SDL_LoadPNG(const char* path){(void)path;loads++;return calloc(1,sizeof(SDL_Surface));}
static SDL_Texture* SDL_CreateTextureFromSurface(void* r,SDL_Surface* s){(void)r;(void)s;live++;if(live>peak)peak=live;return calloc(1,sizeof(SDL_Texture));}
static void SDL_DestroySurface(SDL_Surface* s){free(s);}
static void SDL_DestroyTexture(SDL_Texture* t){if(t){live--;free(t);}}
static void SDL_SetTextureScaleMode(SDL_Texture* t,int mode){(void)t;(void)mode;}
''' + s[:s.index('static bool map_cache_prepare')] + s[start:end] + r'''
int main(void){
 for(int pass=0;pass<3;pass++) {
  for(int y=0;y<18;y++)for(int x=0;x<35;x++)assert(map_cache_tile(x,y,3));
  assert(loads==630);
 }
 assert(live==630);
 for(int level=2;level>=0;level--){
  int capacity=32<<(2*level);
  for(int x=0;x<capacity;x++)assert(map_cache_tile(x,0,level));
  int before=loads;
  for(int x=0;x<capacity;x++)assert(map_cache_tile(x,0,level));
  assert(loads==before&&live==capacity);
  assert(map_cache_tile(capacity,0,level));assert(live==capacity);
 }
 map_cache_clear();assert(live==0);
 puts("PASS: 630 visible map tiles remain resident, level changes release textures, cache capacity stays bounded");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(code)
 subprocess.run([os.environ.get('CC','cc'),'-std=c17',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
