#!/usr/bin/env python3
"""Exercise production asynchronous PNG loading, cross-zoom residency and cleanup."""
from pathlib import Path
import os
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / 'source/src/ui/modern_map_cache.inc').read_text()
code = r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdatomic.h>
#include <pthread.h>
#include <time.h>
typedef uint64_t Uint64;
typedef struct {int w,h;} SDL_Texture;
typedef struct {int w,h;} SDL_Surface;
typedef struct {int value;} TownMapInfo;
typedef pthread_mutex_t SDL_Mutex;
typedef pthread_cond_t SDL_Condition;
typedef struct {pthread_t id;int(*fn)(void*);void* data;} SDL_Thread;
#define SDL_SCALEMODE_LINEAR 1
static void* renderer;
static pthread_t main_thread;
static _Atomic int loads,surfaces,gate;
static int live;
static bool fail_load;
static Uint64 SDL_GetTicksNS(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (Uint64)t.tv_sec*1000000000+t.tv_nsec;}
static void SDL_Delay(int ms){struct timespec t={0,ms*1000000};nanosleep(&t,NULL);}
static SDL_Mutex* SDL_CreateMutex(void){SDL_Mutex* m=malloc(sizeof(*m));pthread_mutex_init(m,NULL);return m;}
static void SDL_LockMutex(SDL_Mutex* m){pthread_mutex_lock(m);}
static void SDL_UnlockMutex(SDL_Mutex* m){pthread_mutex_unlock(m);}
static void SDL_DestroyMutex(SDL_Mutex* m){if(m){pthread_mutex_destroy(m);free(m);}}
static SDL_Condition* SDL_CreateCondition(void){SDL_Condition* c=malloc(sizeof(*c));pthread_cond_init(c,NULL);return c;}
static void SDL_WaitCondition(SDL_Condition* c,SDL_Mutex* m){pthread_cond_wait(c,m);}
static void SDL_SignalCondition(SDL_Condition* c){pthread_cond_signal(c);}
static void SDL_DestroyCondition(SDL_Condition* c){if(c){pthread_cond_destroy(c);free(c);}}
static void* thread_main(void* data){SDL_Thread* t=data;t->fn(t->data);return NULL;}
static SDL_Thread* SDL_CreateThread(int(*fn)(void*),const char* name,void* data){(void)name;SDL_Thread* t=malloc(sizeof(*t));t->fn=fn;t->data=data;assert(!pthread_create(&t->id,NULL,thread_main,t));return t;}
static void SDL_WaitThread(SDL_Thread* t,int* result){(void)result;pthread_join(t->id,NULL);free(t);}
static SDL_Surface* SDL_LoadPNG(const char* path){
 assert(!pthread_equal(main_thread,pthread_self()));
 while(atomic_load(&gate))SDL_Delay(1);
 loads++;if(fail_load)return NULL;
 int level,x,y;assert(sscanf(path,"/%d_%d_%d.png",&level,&x,&y)==3);
 SDL_Surface* s=malloc(sizeof(*s));s->w=s->h=1024>>level;surfaces++;return s;
}
static SDL_Texture* SDL_CreateTextureFromSurface(void* r,SDL_Surface* s){(void)r;assert(pthread_equal(main_thread,pthread_self()));live++;SDL_Texture* t=malloc(sizeof(*t));t->w=s->w;t->h=s->h;return t;}
static void SDL_DestroySurface(SDL_Surface* s){if(s){surfaces--;free(s);}}
static void SDL_DestroyTexture(SDL_Texture* t){assert(pthread_equal(main_thread,pthread_self()));if(t){live--;free(t);}}
static void SDL_SetTextureScaleMode(SDL_Texture* t,int mode){(void)t;(void)mode;}
''' + source[:source.index('static bool map_cache_prepare')] + r'''
static SDL_Texture* get(int x,int y,int level){
 for(int retry=0;retry<5000;retry++){
  map_cache_pump();map_cache_tile(x,y,level);
  assert(map_cache_bytes<=MAP_CACHE_BYTES);
  for(int i=0;i<MAP_CACHE_MAX_TILES;i++)if(map_cache_tiles[i].used&&map_cache_tiles[i].x==x&&map_cache_tiles[i].y==y&&map_cache_tiles[i].level==level&&map_cache_tiles[i].texture)return map_cache_tiles[i].texture;
  SDL_Delay(1);
 }
 assert(!"loader did not finish");return NULL;
}
int main(void){
 main_thread=pthread_self();map_cache_available=true;gate=1;assert(map_cache_start());
 for(int x=0;x<40;x++)assert(!map_cache_tile(x,0,3));
 int pending=0;for(int i=0;i<MAP_CACHE_MAX_TILES;i++)pending+=map_cache_tiles[i].pending;
 assert(pending==MAP_CACHE_JOBS&&loads==0); /* UI never waits for a blocked decoder. */
 gate=0;
 for(int y=0;y<18;y++)for(int x=0;x<35;x++)get(x,y,3);
 assert(loads==630);
 for(int pass=0;pass<3;pass++)for(int y=0;y<18;y++)for(int x=0;x<35;x++)assert(map_cache_tile(x,y,3));
 assert(loads==630&&live==630);
 /* Another zoom level keeps existing textures and provides a fallback. */
 assert(map_cache_tile(0,0,2));
 for(int x=0;x<50;x++)get(x,0,2);
 int before=loads;
 for(int y=0;y<18;y++)for(int x=0;x<35;x++)assert(map_cache_tile(x,y,3));
 assert(loads==before);
 for(int x=0;x<40;x++)get(x,0,0);
 assert(map_cache_bytes<=MAP_CACHE_BYTES);
 /* Map changes/exit cancel queued work and reclaim surfaces and textures. */
 for(int x=100;x<108;x++)map_cache_tile(x,0,0);
 map_cache_clear();assert(live==0&&surfaces==0&&!map_loader.thread&&map_cache_bytes==0);
 map_cache_clear(); /* idempotent after initialization failure or shutdown */
 fail_load=true;map_cache_available=true;assert(map_cache_start());map_cache_tile(1,1,3);
 for(int i=0;i<5000&&map_cache_available;i++){map_cache_pump();map_cache_tile(1,1,3);SDL_Delay(1);}
 assert(!map_cache_available);map_cache_clear();assert(!live&&!surfaces);
 puts("PASS: nonblocking bounded loader, 630 resident tiles, cross-zoom reuse, memory budget, failure and shutdown cleanup");
}
'''
with tempfile.TemporaryDirectory(prefix='arcanum-map-cache-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c17', '-O2', '-pthread',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True, timeout=30)
