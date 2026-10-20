#ifndef MODERN_ENCODING_H
#define MODERN_ENCODING_H

#include <SDL3/SDL.h>

static const unsigned short cp1252[32] = {
    0x20AC,0x81,0x201A,0x192,0x201E,0x2026,0x2020,0x2021,
    0x2C6,0x2030,0x160,0x2039,0x152,0x8D,0x17D,0x8F,
    0x90,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
    0x2DC,0x2122,0x161,0x203A,0x153,0x9D,0x17E,0x178};

static void to_game(const char* src,char* dst,size_t capacity)
{
    if(!capacity)return;
    size_t n=0;
    while(*src && n+1<capacity) {
        Uint32 c=SDL_StepUTF8(&src,NULL);
        if(c<32 || (c>=127 && c<160))continue;
        if(c>255) {
            int index;
            for(index=0;index<32 && cp1252[index]!=c;index++) {}
            if(index==32)continue;
            c=128+index;
        }
        dst[n++]=(char)c;
    }
    dst[n]=0;
}

#endif
