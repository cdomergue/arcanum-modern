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
struct SceneOrigin {
    std::string editor_id,source_path,source_hash;
    std::uint32_t ordinal=0;
    bool mobile=false;
    GameObject object; // Local coordinates; original native ID retained.
};
struct SceneReference {
    std::string id,map,source_path,source_hash;
    std::uint64_t sector=0;
    bool generated=false;
    Bytes ground;
    std::vector<SceneOrigin> origins;
    std::map<std::string,Bytes> resources;
};
Bytes encode_scene_reference(const SceneReference& reference);
std::shared_ptr<SceneReference> decode_scene_reference(const Bytes& bytes);
Bytes encode_scene_selections(const std::vector<GameObject>& global_objects);
void restore_scene_selections(std::vector<GameObject>& global_objects,const Bytes& bytes);
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
    std::shared_ptr<const SceneReference> reference{};
    Bytes update_history{}; // Versioned WorldED authoring JSON; never a game resource.
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
