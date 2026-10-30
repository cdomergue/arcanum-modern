#pragma once
#include "assets.hpp"

namespace worlded {
// Standalone construction study, local to sector 0. No campaign IDs are reserved.
struct BuildingPlan {
    unsigned width = 12, depth = 10, chests = 4;
    unsigned x = 24, y = 24;
    unsigned wall_prototype = 1020, door_prototype = 2028, chest_prototype = 3044;
    unsigned structure = 17, door_art = 13;
    std::uint32_t floor = (4u << 22) | (4u << 16);
    std::uint32_t ground = (1u << 22) | (1u << 16) | 0x1c0;
};
struct Building {
    BuildingPlan plan;
    Sector ground;
    std::vector<GameObject> objects;
    std::map<std::string, Bytes> prototypes;
    std::string map_name = "worlded-scene";
    std::uint64_t sector_id = 0;
    bool existing_map = false;
    std::map<std::string,std::string> location_names{}; // UTF-8, keyed by language.
    std::map<std::string,Bytes> content{}; // scr/, dlg/ and UTF-8 script-sources/ owned by this project.
    Sector sector(std::size_t object_count = SIZE_MAX) const;
};
Building build_house(const Installation& installation, const BuildingPlan& plan = {});
Building insert_room(const Installation& installation,const Building& scene,const BuildingPlan& plan,bool clear_ground);
std::filesystem::path export_building(const Building& building, const std::filesystem::path& new_project);
// Encode new instances using the CE field schema, rejecting unknown/duplicate fields.
Bytes encode_instance(const GameObject& object);
GameObject clone_instance(const GameObject& object);
void set_number(GameObject& object, const std::string& field, std::uint64_t value);
void set_null(GameObject& object, const std::string& field);
}
