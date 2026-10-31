#pragma once
#include "building.hpp"
namespace worlded {
struct WorldSector {
    Sector sector;
    std::string source;
    bool generated;
};
WorldSector read_world_sector(const Installation& installation,const std::string& map,unsigned x,unsigned y,
    const std::map<std::string,Bytes>* overrides=nullptr);
}
