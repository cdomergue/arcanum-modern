#include "worlded/world.hpp"
#include "binary.hpp"
#include <zlib.h>
#include <stdexcept>
namespace worlded {
WorldSector read_world_sector(const Installation& installation,const std::string& map,unsigned x,unsigned y) {
    auto base="maps/"+resource_name(map)+"/";
    auto properties=installation.read(base+"map.prp"); BinaryReader pr(properties); pr.skip(8);
    auto width=pr.u64()/64,height=pr.u64()/64;
    if (x>=width || y>=height || width>16384 || height>16384) throw std::runtime_error("World sector outside map limits");
    auto name=base+std::to_string(std::uint64_t(x)|(std::uint64_t(y)<<26))+".sec";
    if (installation.contains(name)) return {Sector(installation.read(name)),name,false};
    auto data=installation.read(base+"terrain.tdf"); BinaryReader in(data);
    if (in.u32()!=0x3f99999a) throw std::runtime_error("Unsupported terrain version");
    auto flags=in.u32(); if (in.u64()!=width || in.u64()!=height) throw std::runtime_error("Terrain dimensions disagree with map"); in.skip(8);
    Bytes row;
    if (flags&1) {
        for (unsigned r=0;r<=y;++r) {
            auto size=in.u32(); in.need(size);
            if (r==y) { row.resize(width*2); uLongf length=row.size();
                if (uncompress(row.data(),&length,data.data()+in.pos,size)!=Z_OK || length!=row.size()) throw std::runtime_error("Invalid compressed terrain row"); }
            in.skip(size);
        }
    } else { in.skip(y*width*2); row=in.bytes(width*2); }
    unsigned tid=row[x*2]|(unsigned(row[x*2+1])<<8),from=(tid>>11)&31,to=(tid>>6)&31,shape=(tid>>2)&15;
    if (tid==65535) throw std::runtime_error("No terrain at this world location");
    std::map<unsigned,std::string> names;
    for (auto message : read_messages(installation.read("terrain/terrain.mes"))) if (message.id<32) {
        auto flag=message.text.find("/b"); if (flag!=std::string::npos) message.text.resize(flag);
        while (!message.text.empty() && message.text.back()==' ') message.text.pop_back();
        names[message.id]=message.text;
    }
    std::int64_t variant=0; std::string folder;
    if (from==to || shape==0) { unsigned v=tid&3; variant=(v%2)|(std::uint64_t(v/2)<<26); folder=names.at(from); }
    else {
        static const std::int64_t variants[]={-1,1,(2ll<<26)|1,(1ll<<26)|1,(2ll<<26)|3,0,(2ll<<26)|2,-1,3,2,-1,-1,(1ll<<26)|3,-1,-1,-1};
        variant=variants[shape];
        if (variant<0) { std::swap(from,to); variant=variants[15-shape]; }
        if (variant<0) throw std::runtime_error("Unsupported terrain transition");
        folder=names.at(from)+" to "+names.at(to);
    }
    auto source="terrain/"+folder+"/"+std::to_string(variant)+".sec";
    if (!installation.contains(source)) throw std::runtime_error("Missing terrain template: "+source);
    return {Sector(installation.read(source)),source,true};
}
}
