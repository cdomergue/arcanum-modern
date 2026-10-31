#pragma once
#include "binary.hpp"
#include <map>
#include <string>

namespace worlded {
struct Facade {
    unsigned width=0,height=0;
    std::map<std::pair<int,int>,std::uint32_t> tiles;
};
// Native FacWalk records associate ART frames with sparse footprint cells.
// Orientation is baked into the asset; mirrored variants have their own IDs.
inline Facade read_facade(const Bytes& data,unsigned id) {
    BinaryReader in(data);const std::string signature="FacWalk V101  ";
    auto header=in.bytes(signature.size());
    if(std::string(header.begin(),header.end())!=signature || id>=512)
        throw std::runtime_error("Invalid facade signature or ID");
    auto tile=in.u32(),outdoor=in.u32(),flippable=in.u32();
    Facade result;result.width=in.u32();result.height=in.u32();auto count=in.u32();
    if(tile>=64 || outdoor>1 || flippable>1 || !result.width || !result.height
        || result.width>64 || result.height>64 || !count || count>1024)
        throw std::runtime_error("Invalid facade footprint");
    for(unsigned frame=0;frame<count;frame++) {
        auto x=in.u32(),y=in.u32(),walkable=in.u32();
        if(x>=result.width || y>=result.height || walkable>1 || result.tiles.count({int(x),int(y)}))
            throw std::runtime_error("Invalid facade cell");
        result.tiles[{int(x),int(y)}]=0xb0000000u|((id>>8)<<27)|(flippable<<26)
            |(outdoor<<25)|((id&255)<<17)|(tile<<11)|(frame<<1)|walkable;
    }
    if(in.pos!=data.size())throw std::runtime_error("Unexpected facade data");
    return result;
}
}
