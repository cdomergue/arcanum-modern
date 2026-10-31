#include "worlded/formats.hpp"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <zlib.h>

namespace worlded {
namespace fs = std::filesystem;
namespace {
[[noreturn]] void fail(const std::string& message) { throw std::runtime_error(message); }

class Reader {
public:
    explicit Reader(const Bytes& data) : data_(data) {}
    std::size_t pos = 0;
    std::size_t left() const { return data_.size() - pos; }
    void skip(std::size_t size) {
        if (size > left()) fail("Truncated resource at byte " + std::to_string(pos));
        pos += size;
    }
    std::uint32_t u32() {
        const auto start = pos;
        skip(4);
        return std::uint32_t(data_[start]) | (std::uint32_t(data_[start + 1]) << 8)
            | (std::uint32_t(data_[start + 2]) << 16) | (std::uint32_t(data_[start + 3]) << 24);
    }
    std::string str(std::size_t size) {
        const auto start = pos;
        skip(size);
        return std::string(data_.begin() + start, data_.begin() + pos);
    }
    void records(std::uint32_t count, std::size_t size) {
        if (count > left() / size) fail("Record count exceeds resource size");
        skip(count * size);
    }
private:
    const Bytes& data_;
};

Bytes read_range(const fs::path& path, std::uint64_t offset, std::size_t size) {
    if (size > resource_limit) fail("Resource exceeds the 64 MiB limit");
    std::ifstream stream(path, std::ios::binary);
    if (!stream || !stream.seekg(static_cast<std::streamoff>(offset))) fail("Cannot open resource");
    Bytes data(size);
    if (size && !stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(size)))
        fail("Truncated file");
    return data;
}

void put32(Bytes& data, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) data.at(offset + i) = static_cast<std::uint8_t>(value >> (8 * i));
}

const std::string project_marker = "WorldED Renaissance workspace 1\n";

void check_project_ancestors(const fs::path& path) {
    for (auto parent = fs::weakly_canonical(path); !parent.empty();) {
        // Refuse the usual installation roots, even when given a nested path.
        for (const auto& name : {"arcanum1.dat", "Arcanum1.dat", "Arcanum.exe", "arcanum.exe", "arcanum-modern"})
            if (fs::exists(parent / name)) fail("Workspace cannot be inside a game installation");
        const auto next = parent.parent_path();
        if (next == parent) break;
        parent = next;
    }
}

void write_new(const fs::path& path, const Bytes& data) {
    // Atomically reserve a sibling staging directory. Publishing via a hard
    // link fails if the destination appeared meanwhile (no overwrite race).
    const auto staging = fs::path(path.native() + fs::path(".worlded-writing").native());
    if (!fs::create_directory(staging)) fail("Export already in progress or staging path exists");
    const auto temporary = staging / "resource";
    try {
        std::ofstream out(temporary, std::ios::binary);
        if (!out) fail("Cannot create export");
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        out.close();
        if (!out) fail("Cannot finish export");
        fs::create_hard_link(temporary, path);
        fs::remove(temporary);
        fs::remove(staging);
    } catch (...) {
        std::error_code ignored;
        fs::remove(temporary, ignored);
        fs::remove(staging, ignored);
        throw;
    }
}
} // namespace

Bytes read_file(const fs::path& path) {
    const auto size = fs::file_size(path);
    if (size > resource_limit) fail("Resource exceeds the 64 MiB limit");
    return read_range(path, 0, static_cast<std::size_t>(size));
}

std::string cp1252_utf8(const std::string& text) {
    static constexpr std::uint16_t special[] = {
        0x20ac, 0xfffd, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021,
        0x02c6, 0x2030, 0x0160, 0x2039, 0x0152, 0xfffd, 0x017d, 0xfffd,
        0xfffd, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022, 0x2013, 0x2014,
        0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, 0xfffd, 0x017e, 0x0178};
    std::string result;
    for (unsigned char byte : text) {
        unsigned cp = byte >= 0x80 && byte < 0xa0 ? special[byte - 0x80] : byte;
        if (cp < 0x80) result += static_cast<char>(cp);
        else if (cp < 0x800) {
            result += static_cast<char>(0xc0 | (cp >> 6));
            result += static_cast<char>(0x80 | (cp & 63));
        } else {
            result += static_cast<char>(0xe0 | (cp >> 12));
            result += static_cast<char>(0x80 | ((cp >> 6) & 63));
            result += static_cast<char>(0x80 | (cp & 63));
        }
    }
    return result;
}

std::string utf8_cp1252(const std::string& text) {
    // Reverse the presentation mapping, refusing any lossy path conversion.
    std::string result;
    for (std::size_t i = 0; i < text.size();) {
        bool found = false;
        for (unsigned byte = 1; byte < 256; ++byte) {
            if (byte == 0x81 || byte == 0x8d || byte == 0x8f || byte == 0x90 || byte == 0x9d) continue;
            const auto encoded = cp1252_utf8(std::string(1, static_cast<char>(byte)));
            if (text.compare(i, encoded.size(), encoded) == 0) {
                result += static_cast<char>(byte);
                i += encoded.size();
                found = true;
                break;
            }
        }
        if (!found) fail("Resource name cannot be represented in CP1252");
    }
    return result;
}

std::string resource_name(std::string name) {
    if (name.empty()) fail("Empty resource name");
    for (char& c : name) {
        if (c == '\\') c = '/';
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
        if (static_cast<unsigned char>(c) < 32 || c == ':' || c == '\"' || c == '<' || c == '>' || c == '|' || c == '?' || c == '*')
            fail("Unsafe resource name");
    }
    std::size_t start = 0;
    while (start < name.size()) {
        const auto end = name.find('/', start);
        const auto part = name.substr(start, end == std::string::npos ? end : end - start);
        if (part.empty() || part == "." || part == ".." || part.back() == '.' || part.back() == ' ')
            fail("Unsafe resource path component");
        const auto stem = part.substr(0, part.find('.'));
        if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul"
            || (stem.size() == 4 && (stem.substr(0, 3) == "com" || stem.substr(0, 3) == "lpt") && stem[3] >= '0' && stem[3] <= '9'))
            fail("Reserved resource path component");
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (name.back() == '/') fail("Resource name ends with a directory separator");
    return name;
}

Archive::Archive(const fs::path& path) : path_(path), file_size_(fs::file_size(path)) {
    if (file_size_ < 12) fail("Truncated DAT footer");
    const auto footer = read_range(path, file_size_ - 12, 12);
    Reader tail(footer);
    const auto signature = tail.u32();
    if (signature != 0x44415420 && signature != 0x44415431) fail("Unknown DAT signature");
    tail.u32(); // Name allocation hint, not needed by this reader.
    const auto table_distance = tail.u32();
    const std::size_t footer_size = signature == 0x44415431 ? 28 : 12;
    if (file_size_ < footer_size || table_distance < footer_size + 4 || table_distance > file_size_ - 4)
        fail("Invalid DAT table offset");
    const auto table_offset = file_size_ - 4 - table_distance;
    const auto table = read_range(path, table_offset, static_cast<std::size_t>(table_distance) + 4 - footer_size);
    Reader in(table);
    const auto table_size = in.u32();
    if (table_size < 4 || table_size > file_size_ - table_distance) fail("Invalid DAT data offset");
    const auto base = file_size_ - table_distance - table_size;
    const auto count = in.u32();
    if (count > in.left() / 25) fail("Invalid DAT entry count");
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto length = in.u32();
        if (!length || length > 4096) fail("Invalid DAT name length");
        auto name = in.str(length);
        // Original tools sometimes serialized the whole allocated name buffer,
        // including uninitialized bytes after its terminator (UAP hotfix DATs).
        // TIG uses the first NUL while advancing by the full allocation length.
        auto terminator=name.find('\0');
        if(terminator==std::string::npos) fail("Invalid DAT name terminator");
        name.resize(terminator);
        in.u32(); // Original in-memory pointer, ignored on disk.
        ArchiveEntry entry{};
        entry.flags = in.u32();
        entry.size = in.u32();
        entry.packed_size = in.u32();
        entry.offset = base + in.u32();
        if (entry.flags & (0x400 | 0x800)) continue;
        name = resource_name(name);
        const std::uint64_t bytes = entry.flags & 2 ? entry.packed_size : entry.size;
        if (entry.offset > table_offset || bytes > table_offset - entry.offset) fail("DAT entry overlaps index or lies outside file");
        if (!entries_.emplace(name, entry).second) fail("Duplicate DAT resource: " + name);
    }
    if (in.left()) fail("Unexpected bytes after DAT index");
}

Bytes Archive::read(const std::string& name) const {
    if (fs::file_size(path_) != file_size_) fail("Archive changed since opening");
    const auto found = entries_.find(resource_name(name));
    if (found == entries_.end()) fail("Resource not found: " + name);
    const auto& entry = found->second;
    if (entry.size > resource_limit) fail("Resource exceeds the 64 MiB limit");
    const auto raw = read_range(path_, entry.offset, entry.flags & 2 ? entry.packed_size : entry.size);
    if (!(entry.flags & 2)) return raw;
    Bytes output(std::max<std::size_t>(1, entry.size));
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(raw.data());
    stream.avail_in = static_cast<uInt>(raw.size());
    stream.next_out = output.data();
    stream.avail_out = static_cast<uInt>(output.size());
    if (inflateInit(&stream) != Z_OK) fail("Cannot initialize DAT decompressor");
    const int status = inflate(&stream, Z_FINISH);
    const bool valid = status == Z_STREAM_END && stream.total_out == entry.size && stream.total_in == raw.size();
    inflateEnd(&stream);
    if (!valid) fail("Invalid compressed DAT entry");
    output.resize(entry.size);
    return output;
}

Sector::Sector(Bytes data) : original_(std::move(data)) {
    Reader in(original_);
    info_.lights = in.u32();
    in.records(info_.lights, 48);
    tile_offset_ = in.pos;
    for (auto& tile : tiles_) tile = in.u32();
    roof_offset_=in.pos;info_.roofs = in.u32() == 0;
    roofs_.fill(0xffffffff);
    if (info_.roofs) for(auto& roof:roofs_)roof=in.u32();
    version_offset_=in.pos;
    info_.version = in.u32();
    if (info_.version < 0xaa0000 || info_.version > 0xaa0004) fail("Unsupported SEC version");
    if (info_.version >= 0xaa0001) {
        tile_scripts_offset_=in.pos;
        info_.tile_scripts = in.u32();
        for(unsigned i=0;i<info_.tile_scripts;i++) {in.u32();auto tile=in.u32(),flags=in.u32(),counters=in.u32(),number=in.u32();in.u32();scripts_.push_back({tile,flags,counters,number});}
    }
    if (info_.version >= 0xaa0002) {sector_script_offset_=in.pos;auto flags=in.u32(),counters=in.u32(),number=in.u32();if(number)scripts_.push_back({UINT32_MAX,flags,counters,number});}
    if (info_.version >= 0xaa0003) {
        info_.townmap = in.u32();
        settings_offset_=in.pos;
        settings_.aptitude=std::int32_t(in.u32());settings_.light=std::int32_t(in.u32());
        in.u32(); // Sound flags are preserved verbatim.
        settings_.music=std::int32_t(in.u32());settings_.ambient=std::int32_t(in.u32());
    }
    if (info_.version >= 0xaa0004) {blocks_offset_=in.pos;for(auto& block:blocks_)block=in.u32();}
    info_.object_offset = in.pos;
    if (in.left() < 4) fail("Missing SEC object count");
    info_.object_bytes = in.left() - 4;
    in.skip(info_.object_bytes);
    info_.objects = in.u32();
    // An object has at least a version, two IDs and a type. Do not pretend to
    // validate the variable field stream until the object codec is implemented.
    if (info_.objects > info_.object_bytes / 56 || (info_.objects == 0 && info_.object_bytes != 0))
        fail("Inconsistent SEC object count");
}

std::uint32_t Sector::tile(unsigned x, unsigned y) const {
    if (x >= 64 || y >= 64) fail("Tile coordinates must be between 0 and 63");
    return tiles_[x + 64 * y];
}
void Sector::set_tile(unsigned x, unsigned y, std::uint32_t art) {
    tile(x, y); // Bounds check.
    tiles_[x + 64 * y] = art;
}
Bytes Sector::encode() const {
    Bytes result = original_;
    for (std::size_t i = 0; i < tiles_.size(); ++i) put32(result, tile_offset_ + i * 4, tiles_[i]);
    if(info_.roofs)for(std::size_t i=0;i<roofs_.size();++i)put32(result,roof_offset_+4+i*4,roofs_[i]);
    if(blocks_offset_)for(std::size_t i=0;i<blocks_.size();++i)put32(result,blocks_offset_+i*4,blocks_[i]);
    if(settings_offset_) {
        put32(result,settings_offset_,settings_.aptitude);put32(result,settings_offset_+4,settings_.light);
        put32(result,settings_offset_+12,settings_.music);put32(result,settings_offset_+16,settings_.ambient);
    }
    return result;
}
bool Sector::blocked(unsigned x,unsigned y) const {
    tile(x,y);
    unsigned index=y*64+x;
    return (blocks_[index/32] & (1u<<(index%32)))!=0;
}
void Sector::set_blocked(unsigned x,unsigned y,bool value) {
    if(blocked(x,y)==value)return;
    ensure_extended();
    unsigned index=y*64+x,mask=1u<<(index%32);
    if(value)blocks_[index/32]|=mask;else blocks_[index/32]&=~mask;
}
void Sector::set_townmap(std::uint32_t id) {
    if(info_.townmap==id)return;
    ensure_extended();put32(original_,settings_offset_-4,id);info_.townmap=id;
}
void Sector::set_settings(SectorSettings value) {
    if(settings_.aptitude==value.aptitude && settings_.light==value.light && settings_.music==value.music && settings_.ambient==value.ambient)return;
    ensure_extended();settings_=value;
}
void Sector::ensure_extended() {
    if(info_.version<0xaa0004) {
        // Add only the headers absent from this SEC generation. Reparse to
        // update all offsets while retaining object bytes and existing scripts.
        auto data=encode();std::size_t size=512;
        if(info_.version<0xaa0001)size+=4;
        if(info_.version<0xaa0002)size+=12;
        if(info_.version<0xaa0003)size+=24;
        data.insert(data.begin()+info_.object_offset,size,0);
        put32(data,version_offset_,0xaa0004);
        *this=Sector(std::move(data));
    }
}
std::vector<Bytes> Sector::lights() const {
    std::vector<Bytes> result;for(unsigned i=0;i<info_.lights;i++)result.emplace_back(original_.begin()+4+i*48,original_.begin()+4+(i+1)*48);return result;
}
void Sector::set_lights(const std::vector<Bytes>& lights) {
    if(lights.size()>4096)fail("Too many lights");auto data=encode();Bytes header(4,0);put32(header,0,std::uint32_t(lights.size()));
    for(const auto& light:lights){if(light.size()!=48)fail("Invalid light record");header.insert(header.end(),light.begin(),light.end());}
    header.insert(header.end(),data.begin()+4+info_.lights*48,data.end());*this=Sector(std::move(header));
}
void Sector::set_script(SectorScript script) {
    if(script.tile!=UINT32_MAX && script.tile>=4096)fail("Invalid script tile");
    if(!script.number || script.number>INT32_MAX)fail("Invalid script number");
    ensure_extended();
    auto data=encode();std::size_t offset=sector_script_offset_;
    if(script.tile!=UINT32_MAX) {
        std::size_t index=0;
        while(index<info_.tile_scripts && scripts_[index].tile<script.tile)++index;
        offset=tile_scripts_offset_+4+index*24;
        if(index==info_.tile_scripts || scripts_[index].tile!=script.tile) {
            data.insert(data.begin()+offset,24,0);
            put32(data,tile_scripts_offset_,info_.tile_scripts+1);
            put32(data,offset+4,script.tile);
        }
        // Preserve the node flags and serialized next word of existing records.
        offset+=8;
    }
    put32(data,offset,script.flags);put32(data,offset+4,script.counters);put32(data,offset+8,script.number);
    *this=Sector(std::move(data));
}
void Sector::remove_script(std::size_t index) {
    if(index>=scripts_.size())fail("Invalid sector script index");
    auto data=encode();
    if(scripts_[index].tile==UINT32_MAX) {
        if(!sector_script_offset_)fail("Sector script not present");
        std::fill(data.begin()+sector_script_offset_,data.begin()+sector_script_offset_+12,0);
    } else {
        if(!tile_scripts_offset_ || index>=info_.tile_scripts)fail("Tile script not present");
        auto offset=tile_scripts_offset_+4+index*24;
        data.erase(data.begin()+offset,data.begin()+offset+24);
        put32(data,tile_scripts_offset_,info_.tile_scripts-1);
    }
    *this=Sector(std::move(data));
}
std::uint32_t Sector::roof(unsigned x,unsigned y) const {
    if(x>=16 || y>=16)fail("Roof coordinates must be between 0 and 15");
    return roofs_[x+16*y];
}
void Sector::set_roof(unsigned x,unsigned y,std::uint32_t art) {
    roof(x,y);
    if(art!=0xffffffff && (art>>28)!=10)fail("Expected roof art or empty roof");
    if(!info_.roofs && art!=0xffffffff) {
        original_.insert(original_.begin()+roof_offset_+4,1024,255);
        put32(original_,roof_offset_,0);info_.roofs=true;info_.object_offset+=1024;
        version_offset_+=1024;if(blocks_offset_)blocks_offset_+=1024;
        if(settings_offset_)settings_offset_+=1024;
        if(tile_scripts_offset_)tile_scripts_offset_+=1024;
        if(sector_script_offset_)sector_script_offset_+=1024;
    }
    roofs_[x+16*y]=art;
}

ScriptInfo inspect_script(const Bytes& data) {
    Reader in(data);
    in.skip(8); // ScriptHeader (flags, counters).
    ScriptInfo info{};
    info.description = in.str(40);
    const auto nul = info.description.find('\0');
    if (nul != std::string::npos) info.description.resize(nul);
    info.flags = in.u32();
    info.lines = in.u32();
    info.capacity = in.u32();
    in.skip(4); // Original pointer, always 32-bit on disk.
    if (info.lines > info.capacity || info.capacity > resource_limit / 132) fail("Invalid SCR line capacity");
    in.records(info.lines, 132);
    if (in.left()) fail("Unexpected bytes after SCR instructions");
    return info;
}

std::vector<Message> read_messages(const Bytes& data) {
    const std::string text(data.begin(), data.end());
    if (text.find('\0') != std::string::npos) fail("NUL in MES resource");
    std::size_t pos = 0;
    auto field = [&]() {
        const auto start = text.find('{', pos);
        if (start == std::string::npos) fail("Missing MES field");
        const auto end = text.find('}', start + 1);
        if (end == std::string::npos) fail("Unterminated MES field");
        pos = end + 1;
        return text.substr(start + 1, end - start - 1);
    };
    std::vector<Message> result;
    while (text.find('{', pos) != std::string::npos) {
        auto id = field();
        const auto start = id.find_first_not_of(" \t\r\n");
        const auto end = id.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) fail("Empty MES ID");
        id = id.substr(start, end - start + 1);
        std::uint32_t number = 0;
        const auto parsed = std::from_chars(id.data(), id.data() + id.size(), number);
        if (parsed.ec != std::errc{} || parsed.ptr != id.data() + id.size() || number > 0x7fffffff)
            fail("Invalid MES ID");
        result.push_back({number, field()});
    }
    return result;
}

void create_project(const fs::path& path) {
    check_project_ancestors(path);
    if (!fs::create_directory(path)) fail("Project path already exists");
    write_new(path / ".worlded-project", Bytes(project_marker.begin(), project_marker.end()));
}

fs::path export_resource(const fs::path& project, const std::string& relative, const Bytes& data) {
    check_project_ancestors(project);
    if (fs::is_symlink(fs::symlink_status(project)) || fs::is_symlink(fs::symlink_status(project / ".worlded-project")))
        fail("Workspace symlinks are not supported");
    const auto marker = read_file(project / ".worlded-project");
    if (std::string(marker.begin(), marker.end()) != project_marker) fail("Invalid WorldED workspace");
    const auto name = fs::u8path(cp1252_utf8(resource_name(relative)));
    auto parent = project;
    for (const auto& component : name.parent_path()) {
        parent /= component;
        if (fs::is_symlink(fs::symlink_status(parent))) fail("Export path contains a symlink");
        if (!fs::exists(parent)) fs::create_directory(parent);
        if (!fs::is_directory(parent)) fail("Export parent is not a directory");
    }
    const auto destination = project / name;
    write_new(destination, data);
    return destination;
}
} // namespace worlded
