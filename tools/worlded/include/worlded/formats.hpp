#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace worlded {
using Bytes = std::vector<std::uint8_t>;
constexpr std::size_t resource_limit = 64 * 1024 * 1024;

Bytes read_file(const std::filesystem::path& path);
// File names and MES text remain legacy bytes in the format layer. Convert only
// at the presentation boundary; no game text is bundled with the application.
std::string cp1252_utf8(const std::string& text);
std::string utf8_cp1252(const std::string& text);
std::string resource_name(std::string name);

struct ArchiveEntry {
    std::uint32_t flags, size, packed_size;
    std::uint64_t offset;
};
class Archive {
public:
    explicit Archive(const std::filesystem::path& path);
    const std::map<std::string, ArchiveEntry>& entries() const { return entries_; }
    Bytes read(const std::string& name) const;
private:
    std::filesystem::path path_;
    std::uint64_t file_size_;
    std::map<std::string, ArchiveEntry> entries_;
};

struct SectorInfo {
    std::uint32_t version = 0, lights = 0, tile_scripts = 0, objects = 0;
    std::uint32_t townmap = 0;
    bool roofs = false;
    std::size_t object_offset = 0, object_bytes = 0;
};
struct SectorScript {std::uint32_t tile,flags,counters,number;}; // tile == UINT32_MAX: sector attachment
// The sector envelope preserves object bytes. The separate object codec in
// assets.hpp decodes fields; engine checks validate their runtime interpretation.
class Sector {
public:
    explicit Sector(Bytes data);
    const SectorInfo& info() const { return info_; }
    std::uint32_t tile(unsigned x, unsigned y) const;
    void set_tile(unsigned x, unsigned y, std::uint32_t art);
    std::uint32_t roof(unsigned x,unsigned y) const; // 16 x 16, four ground tiles per cell
    void set_roof(unsigned x,unsigned y,std::uint32_t art);
    const std::vector<SectorScript>& scripts() const {return scripts_;}
    void remove_script(std::size_t index); // Remove one tile/sector attachment; never deletes SCR files.
    Bytes encode() const;
private:
    Bytes original_;
    SectorInfo info_;
    std::size_t tile_offset_ = 0;
    std::size_t roof_offset_ = 0;
    std::size_t tile_scripts_offset_ = 0, sector_script_offset_ = 0;
    std::array<std::uint32_t,256> roofs_{};
    std::vector<SectorScript> scripts_;
    std::array<std::uint32_t, 4096> tiles_{};
};

struct ScriptInfo {
    std::string description;
    std::uint32_t flags, lines, capacity;
};
ScriptInfo inspect_script(const Bytes& data);
// Strict numeric IDs, duplicate entries retained in file order. Follows the
// engine's brace scanning, including braces in apparent // comments.
struct Message { std::uint32_t id; std::string text; };
std::vector<Message> read_messages(const Bytes& data);

// A workspace must be new and outside game installations. Exports never replace
// existing files and never follow symlinks inside the workspace.
void create_project(const std::filesystem::path& path);
std::filesystem::path export_resource(const std::filesystem::path& project,
    const std::string& relative, const Bytes& data);
} // namespace worlded
