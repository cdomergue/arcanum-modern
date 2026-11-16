#pragma once
#include "formats.hpp"
#include <memory>

namespace worlded {
struct ArrayElement { std::uint32_t index; Bytes data; };
struct ObjectField {
    std::string name, kind, text;
    bool present = true;
    std::uint64_t number = 0;
    std::vector<ArrayElement> elements;
};
struct GameObject {
    std::uint32_t type = 0, prototype_id = 0;
    bool prototype = false;
    std::string id;
    std::string editor_id; // Selection identity only; never serialized.
    std::size_t offset = 0, size = 0;
    std::vector<ObjectField> fields;
    const ObjectField* field(const std::string& name) const;
    std::uint64_t number(const std::string& name, std::uint64_t fallback = 0) const;
};
const char* object_type_name(unsigned type);
std::vector<GameObject> read_objects(const Bytes& data, std::size_t offset = 0,
    std::size_t end = 0, std::size_t expected = 0);
std::vector<GameObject> sector_objects(const Sector& sector);
std::string object_field_text(const ObjectField& field);

struct ArtFrame {
    std::uint32_t width, height, encoded_size;
    std::int32_t hot_x, hot_y, offset_x, offset_y;
    std::size_t offset = 0;
};
class Art {
public:
    explicit Art(Bytes data);
    unsigned rotations() const { return rotations_; }
    unsigned frames() const { return frames_; }
    unsigned fps() const { return fps_; }
    unsigned flags() const;
    std::int32_t action_frame() const;
    bool palette_available(unsigned palette) const;
    const ArtFrame& frame(unsigned rotation, unsigned index) const;
    Bytes indexed(unsigned rotation, unsigned index) const;
    Bytes rgba(unsigned rotation, unsigned index, unsigned palette = 0) const;
    using Palette = std::array<std::uint32_t, 256>;
    const Palette& palette(unsigned slot) const;
    void set_palette(unsigned slot, const Palette& colors);
    void set_fps(unsigned value);
    void set_action_frame(std::int32_t value);
    void set_anchor(unsigned rotation, unsigned index, std::int32_t x, std::int32_t y);
    void set_offset(unsigned rotation, unsigned index, std::int32_t x, std::int32_t y);
    // Untouched bytes (including pointer stubs, RLE and trailing data) survive edits.
    const Bytes& encode() const { return data_; }
    void validate() const;
    static Art create(unsigned rotations, unsigned fps, const Palette& palette,
        const std::vector<ArtFrame>& frames, const std::vector<Bytes>& pixels);
private:
    Bytes data_;
    unsigned rotations_ = 0, frames_ = 0, fps_ = 0;
    std::array<bool, 4> present_{};
    std::array<std::array<std::uint32_t, 256>, 4> palettes_{};
    std::vector<ArtFrame> frame_data_;
    std::size_t headers_offset_ = 0;
};

struct ResourceSource {
    std::filesystem::path file;
    std::shared_ptr<Archive> archive;
    std::uint64_t size = 0;
    std::shared_ptr<const Bytes> generated;
};
// TIG, loose root resources, base archives, data/, module archive, numerical
// module patches, then module loose data. Records every shadowed source.
class Installation {
public:
    explicit Installation(const std::filesystem::path& game, const std::string& module = "Arcanum");
    const std::map<std::string, std::vector<ResourceSource>>& resources() const { return resources_; }
    Bytes read(const std::string& name) const;
    bool contains(const std::string& name) const;
    // Fill missing IDs only. Installed prototypes, including custom filenames,
    // retain precedence over the engine's defaults.
    std::size_t add_default_prototypes(const std::filesystem::path& directory);
    std::vector<std::string> diagnostics;
private:
    std::map<std::string, std::vector<ResourceSource>> resources_;
    void add_archive(const std::filesystem::path& path);
    void add_directory(const std::filesystem::path& path);
};
} // namespace worlded
