#pragma once
#include "building.hpp"
#include <stdexcept>

namespace worlded {
// Building locations use sector-local coordinates. Routes outside that sector
// retain world coordinates; only coordinates belonging to this sector localize.
inline std::uint64_t npc_route_global(std::uint64_t loc, std::uint64_t sector) {
    std::uint64_t x=std::uint32_t(loc), y=std::uint32_t(loc>>32);
    if(x<64 && y<64) { x+=(sector&0x3ffffff)*64; y+=(sector>>26)*64; }
    if(x>UINT32_MAX || y>UINT32_MAX) throw std::runtime_error("NPC route exceeds world coordinate range");
    return x|(y<<32);
}
inline std::uint64_t npc_route_local(std::uint64_t loc, std::uint64_t sector) {
    const std::uint64_t x=std::uint32_t(loc), y=std::uint32_t(loc>>32);
    if(x/64==(sector&0x3ffffff) && y/64==(sector>>26)) return (x%64)|((y%64)<<32);
    return loc;
}
inline void transform_npc_routes(GameObject& object,std::uint64_t sector,bool to_world) {
    if(object.type!=16) return;
    auto convert=[&](std::uint64_t loc){return to_world?npc_route_global(loc,sector):npc_route_local(loc,sector);};
    for(auto name:{"CRITTER_TELEPORT_DEST","NPC_STANDPOINT_DAY","NPC_STANDPOINT_NIGHT"})
        if(auto field=object.field(name);field && field->present) set_number(object,name,convert(object.number(name)));
    for(auto& field:object.fields) if(field.name=="NPC_WAYPOINTS_IDX" && field.present) for(auto& entry:field.elements) {
        if(entry.data.size()!=8) throw std::runtime_error("Invalid NPC waypoint record");
        std::uint64_t loc=0;for(unsigned i=0;i<8;i++)loc|=std::uint64_t(entry.data[i])<<(8*i);
        loc=convert(loc);for(unsigned i=0;i<8;i++)entry.data[i]=std::uint8_t(loc>>(8*i));
    }
}
}
