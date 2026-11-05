"""Compile the production crop/quantization code; no SDL installation needed."""
from pathlib import Path
import base64,os,struct,subprocess,tempfile,zlib
ROOT=Path(__file__).resolve().parents[1]
JPEG='/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAIBAQEBAQIBAQECAgICAgQDAgICAgUEBAMEBgUGBgYFBgYGBwkIBgcJBwYGCAsICQoKCgoKBggLDAsKDAkKCgr/2wBDAQICAgICAgUDAwUKBwYHCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgoKCgr/wAARCAACAAMDASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDxOiiiv5fP78P/2Q=='
SHIM=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
typedef uint8_t Uint8;
typedef struct {Uint8 r,g,b,a;} SDL_Color;
typedef struct {SDL_Color colors[256];} SDL_Palette;
typedef struct {int w,h,pitch;void* pixels;SDL_Palette* palette;} SDL_Surface;
typedef struct {int type;uint64_t size;} SDL_PathInfo;
#define SDL_PATHTYPE_FILE 1
#define SDL_PIXELFORMAT_INDEX8 1
#define SDL_malloc malloc
#define SDL_free free
static bool SDL_GetPathInfo(const char* p,SDL_PathInfo* i){struct stat s;if(stat(p,&s))return false;i->size=s.st_size;i->type=S_ISREG(s.st_mode)?1:0;return true;}
static void* SDL_LoadFile(const char* p,size_t* n){FILE* f=fopen(p,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);*n=ftell(f);rewind(f);void* d=malloc(*n+1);assert(d);assert(fread(d,1,*n,f)==*n);fclose(f);return d;}
static SDL_Surface* SDL_CreateSurface(int w,int h,int fmt){(void)fmt;SDL_Surface* s=calloc(1,sizeof(*s));s->w=w;s->h=h;s->pitch=w;s->pixels=calloc(w*h,1);return s;}
static SDL_Palette* SDL_CreateSurfacePalette(SDL_Surface* s){return s->palette=calloc(1,sizeof(SDL_Palette));}
static bool SDL_SetPaletteColors(SDL_Palette* p,const SDL_Color* c,int start,int count){memcpy(p->colors+start,c,count*sizeof(*c));return true;}
static void SDL_DestroySurface(SDL_Surface* s){if(s){free(s->palette);free(s->pixels);free(s);}}
#include "IMAGE_SOURCE"
static SDL_Color sample(SDL_Surface* s,int x,int y){return s->palette->colors[((Uint8*)s->pixels)[y*s->pitch+x]];}
int main(void){
 int w,h;unsigned char* p=portrait_image_load("test.png",&w,&h);assert(p&&w==4&&h==2);
 SDL_Surface* crop=portrait_image_crop(p,w,h,2,0,2,64);assert(crop);SDL_Color c=sample(crop,32,32);assert(c.b>240&&c.r<10);SDL_DestroySurface(crop);stbi_image_free(p);
 p=portrait_image_load("test.jpg",&w,&h);assert(p&&w==3&&h==2);assert(abs(p[0]-190)<8);stbi_image_free(p);
 p=portrait_image_load("test.bmp",&w,&h);assert(p&&w==2&&h==2);stbi_image_free(p);
 assert(!portrait_image_load("bad.png",&w,&h));assert(!portrait_image_load("huge.png",&w,&h));assert(!portrait_image_load("missing.png",&w,&h));
 unsigned char transparent[4]={255,0,0,0};crop=portrait_image_crop(transparent,1,1,0,0,1,128);c=sample(crop,50,50);assert(c.r==30&&c.g==22&&c.b==16);SDL_DestroySurface(crop);
 unsigned char gradient[128*128*4];
 for(int y=0;y<128;y++)for(int x=0;x<128;x++){int n=(y*128+x)*4;gradient[n]=x*2;gradient[n+1]=y*2;gradient[n+2]=100;gradient[n+3]=255;}
 crop=portrait_image_crop(gradient,128,128,0,0,128,128);double error=0;
 for(int y=0;y<128;y++)for(int x=0;x<128;x++){c=sample(crop,x,y);error+=abs(c.r-x*2)+abs(c.g-y*2)+abs(c.b-100);}
 assert(error/(128*128*3)<5);SDL_DestroySurface(crop);
 puts("PASS: PNG/JPEG/BMP decoding, invalid and oversized images, crop coordinates, alpha matte and adaptive palette accuracy");
}
'''

def png(width,height,pixels):
 def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data))
 return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(pixels))+chunk(b'IEND',b'')
with tempfile.TemporaryDirectory(prefix='arcanum-portrait-') as work:
 p=Path(work)
 p.joinpath('test.png').write_bytes(png(4,2,(b'\0'+bytes([255,0,0,255])*2+bytes([0,0,255,255])*2)*2))
 p.joinpath('huge.png').write_bytes(png(9000,9000,b''))
 p.joinpath('bad.png').write_bytes(b'invalid')
 p.joinpath('test.jpg').write_bytes(base64.b64decode(JPEG))
 # 2x2 uncompressed 24-bit BMP, four bytes of row padding in total.
 p.joinpath('test.bmp').write_bytes(b'BM'+struct.pack('<IHHI',70,0,0,54)+struct.pack('<IiiHHIIiiII',40,2,2,1,24,0,16,0,0,0,0)+bytes([30,70,190]*2+[0,0])*2)
 p.joinpath('test.c').write_text(SHIM.replace('IMAGE_SOURCE',(ROOT/'source/src/ui/modern_portrait_image.inc').as_posix()))
 subprocess.run([os.environ.get('CC','cc'),'-std=c17','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
