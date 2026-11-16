#pragma once
#include "worlded/assets.hpp"
#include <QImage>
#include <algorithm>
#include <limits>
#include <map>

namespace worlded {
inline QImage art_image(const Art& art, unsigned rotation, unsigned frame, unsigned palette=0) {
    const auto& f=art.frame(rotation,frame); auto rgba=art.rgba(rotation,frame,palette);
    return QImage(rgba.data(),int(f.width),int(f.height),int(f.width)*4,QImage::Format_RGBA8888).copy();
}

// One weighted median-cut palette for the whole animation; never quantize frames
// separately, which would change the meaning of an index between frames.
inline Art art_from_pngs(const std::vector<QImage>& images, unsigned rotations, unsigned fps) {
    if ((rotations!=1 && rotations!=8) || images.empty() || images.size()%rotations || images.size()/rotations>4096)
        throw std::runtime_error("Select an equal number of PNG frames for each direction (1 or 8 directions)");
    std::map<QRgb,std::uint64_t> histogram;
    bool reducedColors=false;
    std::uint64_t total=0;
    for (const auto& image:images) {
        total+=std::uint64_t(image.width())*image.height();
        if (image.isNull() || image.width()>16384 || image.height()>16384 || total>resource_limit/4)
            throw std::runtime_error("Invalid or excessive PNG animation dimensions");
        for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) {
            auto c=image.pixel(x,y);
            if (qAlpha(c)<128) continue;
            ++histogram[c&(reducedColors?0xf8f8f8:0xffffff)];
            if (!reducedColors && histogram.size()>65536) {
                std::map<QRgb,std::uint64_t> reduced;
                for (auto [rgb,count]:histogram) reduced[rgb&0xf8f8f8]+=count;
                histogram.swap(reduced);reducedColors=true;
            }
        }
    }
    struct Color { QRgb rgb; std::uint64_t count; };
    std::vector<Color> colors;
    // Bound the quantizer's memory for photographs while preserving small palettes exactly.
    for (auto [rgb,count]:histogram) colors.push_back({rgb,count});
    Art::Palette palette{};
    std::size_t used=colors.size();
    if (colors.size()<=255) {
        for (std::size_t i=0;i<colors.size();++i) palette[i+1]=colors[i].rgb;
    } else {
        struct Box { std::size_t begin,end; };
        std::vector<Box> boxes{{0,colors.size()}};
        auto component=[](QRgb c,int axis){return int((c>>(axis*8))&255);};
        while (boxes.size()<255) {
            int best=-1,axis=0,spread=-1;
            for (std::size_t b=0;b<boxes.size();++b) {
                if (boxes[b].end-boxes[b].begin<2) continue;
                for (int a=0;a<3;++a) {
                    int lo=255,hi=0;
                    for (auto i=boxes[b].begin;i<boxes[b].end;++i) { auto c=component(colors[i].rgb,a);lo=std::min(lo,c);hi=std::max(hi,c); }
                    if (hi-lo>spread) {best=int(b);axis=a;spread=hi-lo;}
                }
            }
            if (best<0) break;
            auto box=boxes[best];
            std::sort(colors.begin()+box.begin,colors.begin()+box.end,[&](Color a,Color b){
                auto ac=component(a.rgb,axis),bc=component(b.rgb,axis);return ac==bc?a.rgb<b.rgb:ac<bc;
            });
            std::uint64_t count=0,part=0;
            for (auto i=box.begin;i<box.end;++i) count+=colors[i].count;
            auto split=box.begin;
            do {part+=colors[split++].count;} while (split<box.end-1 && part<count/2);
            boxes[best].end=split;boxes.push_back({split,box.end});
        }
        used=boxes.size();
        for (std::size_t b=0;b<boxes.size();++b) {
            std::uint64_t r=0,g=0,bl=0,count=0;
            for (auto i=boxes[b].begin;i<boxes[b].end;++i) {
                auto c=colors[i];count+=c.count;r+=qRed(c.rgb)*c.count;g+=qGreen(c.rgb)*c.count;bl+=qBlue(c.rgb)*c.count;
            }
            palette[b+1]=std::uint32_t(((r/count)<<16)|((g/count)<<8)|(bl/count));
        }
    }
    std::map<QRgb,std::uint8_t> lookup;
    std::vector<ArtFrame> frames; std::vector<Bytes> pixels;
    for (const auto& image:images) {
        frames.push_back({unsigned(image.width()),unsigned(image.height()),0,image.width()/2,image.height()-1,0,0});
        Bytes indices;indices.reserve(std::size_t(image.width())*image.height());
        for (int y=0;y<image.height();++y) for (int x=0;x<image.width();++x) {
            auto color=image.pixel(x,y);
            if (qAlpha(color)<128) {indices.push_back(0);continue;}
            color&=reducedColors?0xf8f8f8:0xffffff;
            auto found=lookup.find(color);
            if (found==lookup.end()) {
                int best=1,distance=std::numeric_limits<int>::max();
                for (std::size_t p=1;p<=used;++p) {
                    int r=qRed(color)-qRed(palette[p]),g=qGreen(color)-qGreen(palette[p]),b=qBlue(color)-qBlue(palette[p]);
                    auto d=r*r+g*g+b*b;if (d<distance) {distance=d;best=int(p);}
                }
                found=lookup.emplace(color,std::uint8_t(best)).first;
            }
            indices.push_back(found->second);
        }
        pixels.push_back(std::move(indices));
    }
    return Art::create(rotations,fps,palette,frames,pixels);
}
}
