/* Image processing for native portraits. stb_image 2.30, upstream revision
 * 013ac3beddff3dbffafd5177e7972067cd2b5083 (license in the vendored header). */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_NO_STDIO
#define STBI_MAX_DIMENSIONS 8192
#include "../../third_party/stb/stb_image.h"

static unsigned char* portrait_image_load(const char* path,int* width,int* height)
{
    SDL_PathInfo info;
    if(!SDL_GetPathInfo(path,&info) || info.type!=SDL_PATHTYPE_FILE || info.size>64*1024*1024)return NULL;
    size_t length;void* bytes=SDL_LoadFile(path,&length);
    if(!bytes)return NULL;
    int channels,w,h;unsigned char* pixels=NULL;
    if(length<=64*1024*1024 && stbi_info_from_memory(bytes,(int)length,&w,&h,&channels)
        && w>0 && h>0 && w<=8192 && h<=8192 && (int64_t)w*h<=24*1024*1024)
        pixels=stbi_load_from_memory(bytes,(int)length,width,height,&channels,4);
    SDL_free(bytes);return pixels;
}

typedef struct PortraitColor {unsigned char r,g,b;} PortraitColor;
typedef struct PortraitColorBox {int first,count,channel,range;} PortraitColorBox;
static int portrait_color_channel;
static int portrait_color_compare(const void* a,const void* b)
{
    const unsigned char* p=a;const unsigned char* q=b;
    int delta=p[portrait_color_channel]-q[portrait_color_channel];
    return delta?delta:memcmp(a,b,sizeof(PortraitColor));
}
static void portrait_color_bounds(PortraitColor* pixels,PortraitColorBox* box)
{
    int lo[3]={255,255,255},hi[3]={0,0,0};
    for(int n=box->first;n<box->first+box->count;n++)for(int c=0;c<3;c++) {
        int v=((unsigned char*)&pixels[n])[c];if(v<lo[c])lo[c]=v;if(v>hi[c])hi[c]=v;
    }
    box->range=0;box->channel=0;
    for(int c=0;c<3;c++)if(hi[c]-lo[c]>box->range){box->range=hi[c]-lo[c];box->channel=c;}
}
static SDL_Surface* portrait_image_indexed(const PortraitColor* pixels,int size)
{
    int total=size*size;
    PortraitColor* sorted=SDL_malloc((size_t)total*sizeof(*sorted));
    if(!sorted)return NULL;
    memcpy(sorted,pixels,(size_t)total*sizeof(*sorted));
    PortraitColorBox boxes[256]={{0,total,0,0}};int count=1;
    portrait_color_bounds(sorted,&boxes[0]);
    while(count<256) {
        int best=-1;double score=0;
        for(int n=0;n<count;n++) {
            double candidate=boxes[n].range*sqrt((double)boxes[n].count);
            if(boxes[n].count>1 && candidate>score){score=candidate;best=n;}
        }
        if(best<0)break;
        PortraitColorBox* box=&boxes[best];portrait_color_channel=box->channel;
        qsort(sorted+box->first,box->count,sizeof(*sorted),portrait_color_compare);
        int half=box->count/2;
        boxes[count]=(PortraitColorBox){box->first+half,box->count-half,0,0};box->count=half;
        portrait_color_bounds(sorted,box);portrait_color_bounds(sorted,&boxes[count++]);
    }
    SDL_Color colors[256];
    for(int n=0;n<256;n++)colors[n]=(SDL_Color){0,0,0,255};
    for(int n=0;n<count;n++) {
        unsigned sum[3]={0};PortraitColorBox* box=&boxes[n];
        for(int p=box->first;p<box->first+box->count;p++) {
            sum[0]+=sorted[p].r;sum[1]+=sorted[p].g;sum[2]+=sorted[p].b;
        }
        colors[n]=(SDL_Color){(Uint8)((sum[0]+box->count/2)/box->count),
            (Uint8)((sum[1]+box->count/2)/box->count),(Uint8)((sum[2]+box->count/2)/box->count),255};
    }
    SDL_free(sorted);
    SDL_Surface* result=SDL_CreateSurface(size,size,SDL_PIXELFORMAT_INDEX8);
    if(!result)return NULL;
    SDL_Palette* palette=SDL_CreateSurfacePalette(result);
    if(!palette || !SDL_SetPaletteColors(palette,colors,0,256)){SDL_DestroySurface(result);return NULL;}
    for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
        PortraitColor p=pixels[y*size+x];int best=0,distance=INT_MAX;
        for(int n=0;n<count;n++) {
            int r=p.r-colors[n].r,g=p.g-colors[n].g,b=p.b-colors[n].b;
            int d=r*r+g*g+b*b;
            if(d<distance){distance=d;best=n;}
        }
        ((Uint8*)result->pixels)[y*result->pitch+x]=(Uint8)best;
    }
    return result;
}

/* Square crop in source coordinates, sampled before palette reduction. */
static SDL_Surface* portrait_image_crop(const unsigned char* rgba,int width,int height,
    float left,float top,float side,int size)
{
    if(!rgba || width<1 || height<1 || side<=0 || (size!=64 && size!=128))return NULL;
    PortraitColor* pixels=SDL_malloc((size_t)size*size*sizeof(*pixels));
    if(!pixels)return NULL;
    int samples=(int)ceilf(side/size);if(samples<1)samples=1;if(samples>8)samples=8;
    const int matte[3]={30,22,16};
    for(int y=0;y<size;y++)for(int x=0;x<size;x++) {
        double sum[3]={0};
        for(int sy=0;sy<samples;sy++)for(int sx=0;sx<samples;sx++) {
            float fx=left+(x+(sx+.5f)/samples)*side/size-.5f;
            float fy=top+(y+(sy+.5f)/samples)*side/size-.5f;
            fx=fmaxf(0,fminf(width-1.f,fx));fy=fmaxf(0,fminf(height-1.f,fy));
            int ix=(int)fx,iy=(int)fy;float dx=fx-ix,dy=fy-iy;
            for(int yy=0;yy<2;yy++)for(int xx=0;xx<2;xx++) {
                int px=ix+xx,py=iy+yy;if(px>=width)px=width-1;if(py>=height)py=height-1;
                const unsigned char* p=rgba+((size_t)py*width+px)*4;
                float weight=(xx?dx:1-dx)*(yy?dy:1-dy),alpha=p[3]/255.f;
                for(int c=0;c<3;c++)sum[c]+=(p[c]*alpha+matte[c]*(1-alpha))*weight;
            }
        }
        unsigned char* out=(unsigned char*)&pixels[y*size+x];
        for(int c=0;c<3;c++)out[c]=(unsigned char)fmin(255,fmax(0,sum[c]/(samples*samples)+.5));
    }
    SDL_Surface* result=portrait_image_indexed(pixels,size);SDL_free(pixels);return result;
}
