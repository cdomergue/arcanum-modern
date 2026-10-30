#include "worlded/building.hpp"
#include "binary.hpp"
#include <sstream>
#include <iomanip>

namespace worlded {
namespace {
struct FieldSpec { const char* name; const char* kind; };
#include "object_schema.inc"
const char* type_names[] = {"WALL", "PORTAL", "CONTAINER", "SCENERY", "PROJECTILE",
    "WEAPON", "AMMO", "ARMOR", "GOLD", "FOOD", "SCROLL", "KEY", "KEY_RING", "WRITTEN", "GENERIC", "PC", "NPC", "TRAP"};
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
std::string identity(const Bytes& bytes) {
    std::ostringstream out;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        if (i < 2 || i >= 8) out << std::hex << std::setfill('0') << std::setw(2) << unsigned(bytes[i]);
    return out.str();
}
ObjectField read_field(BinaryReader& in, const FieldSpec& spec) {
    ObjectField field;
    field.name = spec.name; field.kind = spec.kind;
    if (field.kind == "INT32") { field.number = in.u32(); return field; }
    auto presence = in.u8();
    require(presence <= 1, "Invalid object field presence");
    field.present = presence != 0;
    if (!presence) return field;
    if (field.kind == "INT64") field.number = in.u64();
    else if (field.kind == "HANDLE") field.text = identity(in.bytes(24));
    else if (field.kind == "STRING") {
        auto length = in.u32();
        auto text = in.bytes(std::size_t(length) + 1);
        require(text.back() == 0, "Unterminated object string");
        field.text.assign(text.begin(), text.end() - 1);
    } else {
        static const std::map<std::string, unsigned> sizes = {{"INT32_ARRAY",4},{"UINT32_ARRAY",4},
            {"INT64_ARRAY",8},{"UINT64_ARRAY",8},{"SCRIPT_ARRAY",12},{"QUEST_ARRAY",16},{"HANDLE_ARRAY",24}};
        auto kind = sizes.find(field.kind);
        require(kind != sizes.end(), "Unsupported object field kind");
        auto size = in.u32(), count = in.u32();
        in.u32(); // In-memory bitset ID.
        require(size == kind->second, "Invalid object array element size");
        auto values = in.pos;
        in.records(count, size);
        auto words = in.u32();
        require(words <= (in.end - in.pos) / 4, "Truncated object array mask");
        std::size_t rank = 0;
        for (std::uint32_t word = 0; word < words; ++word) {
            auto mask = in.u32();
            for (unsigned bit = 0; bit < 32; ++bit) if (mask & (1u << bit)) {
                require(rank < count, "Object array count and mask disagree");
                auto begin = values + rank++ * size;
                field.elements.push_back({word * 32 + bit, Bytes(in.data.begin() + begin, in.data.begin() + begin + size)});
            }
        }
        require(rank == count, "Object array count and mask disagree");
    }
    return field;
}
}
const char* object_type_name(unsigned type) {
    return type < 18 ? type_names[type] : "UNKNOWN";
}
const ObjectField* GameObject::field(const std::string& name) const {
    for (const auto& value : fields) if (value.name == name) return &value;
    return nullptr;
}
std::uint64_t GameObject::number(const std::string& name, std::uint64_t fallback) const {
    auto value = field(name);
    return value && value->present ? value->number : fallback;
}
std::vector<GameObject> read_objects(const Bytes& data, std::size_t offset, std::size_t end, std::size_t expected) {
    BinaryReader in(data, offset, end);
    std::vector<GameObject> result;
    while (in.pos < in.end) {
        GameObject obj;
        obj.offset = in.pos;
        require(in.u32() == 119, "Unsupported object version (expected 119)");
        auto proto = in.bytes(24);
        BinaryReader pr(proto);
        auto proto_type = pr.u16(); pr.skip(6); obj.prototype_id = pr.u32();
        obj.prototype = proto_type == 0xffff;
        require(obj.prototype || proto_type == 1, "Invalid prototype identifier");
        auto id = in.bytes(24);
        require((id[0] <= 3 && id[1] == 0), "Invalid persistent object identifier");
        obj.id = identity(id);
        if (obj.prototype) {
            BinaryReader own_id(id); own_id.skip(8); obj.prototype_id = own_id.u32();
        }
        obj.type = in.u32();
        require(obj.type < 18, "Unsupported object type");
        std::vector<std::string> groups = {"BASE"};
        if (obj.type >= 5 && obj.type <= 14) groups.emplace_back("ITEM");
        if (obj.type == 15 || obj.type == 16) groups.emplace_back("CRITTER");
        groups.emplace_back(type_names[obj.type]);
        unsigned words = 0;
        for (const auto& group : groups) words += (field_groups.at(group).size() + 31) / 32;
        unsigned count = obj.prototype ? 0 : in.u16();
        std::vector<std::uint32_t> masks;
        for (unsigned i = 0; i < words; ++i) masks.push_back(in.u32());
        unsigned word = 0;
        for (const auto& group : groups) {
            const auto& specs = field_groups.at(group);
            if (!obj.prototype && specs.size() % 32)
                require((masks[word + specs.size() / 32] >> (specs.size() % 32)) == 0, "Unknown object field bits");
            for (std::size_t i = 0; i < specs.size(); ++i)
                if (obj.prototype || (masks[word + i / 32] & (1u << (i % 32)))) obj.fields.push_back(read_field(in, specs[i]));
            word += (specs.size() + 31) / 32;
        }
        require(obj.prototype || obj.fields.size() == count, "Object field count disagrees with mask");
        obj.size = in.pos - obj.offset;
        result.push_back(std::move(obj));
        require(result.size() <= 100000, "Too many objects in one resource");
    }
    require(!expected || result.size() == expected, "Sector object count disagrees with payload");
    return result;
}
std::vector<GameObject> sector_objects(const Sector& sector) {
    const auto& info = sector.info();
    if (!info.objects) return {};
    return read_objects(sector.encode(), info.object_offset, info.object_offset + info.object_bytes, info.objects);
}
std::string object_field_text(const ObjectField& field) {
    if (!field.present) return "null";
    if (field.kind == "INT32") return std::to_string(static_cast<std::int32_t>(field.number));
    if (field.kind == "INT64") return std::to_string(field.number);
    if (field.kind == "STRING") return cp1252_utf8(field.text);
    if (field.kind == "HANDLE") return field.text;
    return std::to_string(field.elements.size()) + " elements";
}
}

namespace worlded {
namespace {
std::vector<std::string> groups_for(unsigned type) {
    require(type < 18, "Unsupported object type");
    std::vector<std::string> groups{"BASE"};
    if (type >= 5 && type <= 14) groups.emplace_back("ITEM");
    if (type == 15 || type == 16) groups.emplace_back("CRITTER");
    groups.emplace_back(type_names[type]); return groups;
}
ObjectField& mutable_field(GameObject& object, const std::string& name) {
    for (auto& f : object.fields) if (f.name == name) return f;
    for (const auto& group : groups_for(object.type)) for (const auto& spec : field_groups.at(group))
        if (name == spec.name) { ObjectField f; f.name=name; f.kind=spec.kind; object.fields.push_back(f); return object.fields.back(); }
    throw std::runtime_error("Unknown object field: " + name);
}
void append_number(Bytes& out, std::uint64_t n, unsigned bytes) {
    for (unsigned i=0;i<bytes;++i) out.push_back(static_cast<std::uint8_t>(n>>(8*i)));
}
void append_id(Bytes& out, const std::string& id) {
    require(id.size()==36, "Invalid object identifier length");
    auto nibble=[](char c)->unsigned {
        if (c>='0' && c<='9') return c-'0';
        if (c>='a' && c<='f') return c-'a'+10;
        throw std::runtime_error("Invalid object identifier hex");
    };
    for (unsigned i=0;i<18;++i) {
        if (i==2) out.insert(out.end(),6,0);
        out.push_back(static_cast<std::uint8_t>((nibble(id[i*2])<<4)|nibble(id[i*2+1])));
    }
}
void write_field(Bytes& out, const ObjectField& f, const FieldSpec& spec) {
    require(f.kind==spec.kind, "Object field kind disagrees with schema");
    if (f.kind=="INT32") { require(f.present && f.number<=UINT32_MAX,"Invalid INT32 field"); append_number(out,f.number,4); return; }
    out.push_back(f.present ? 1 : 0); if (!f.present) return;
    if (f.kind=="INT64") append_number(out,f.number,8);
    else if (f.kind=="HANDLE") append_id(out,f.text);
    else if (f.kind=="STRING") {
        require(f.text.size()<resource_limit,"Object string too long");
        append_number(out,f.text.size(),4); out.insert(out.end(),f.text.begin(),f.text.end()); out.push_back(0);
    } else {
        const std::map<std::string,unsigned> sizes={{"INT32_ARRAY",4},{"UINT32_ARRAY",4},{"INT64_ARRAY",8},{"UINT64_ARRAY",8},{"SCRIPT_ARRAY",12},{"QUEST_ARRAY",16},{"HANDLE_ARRAY",24}};
        auto size=sizes.at(f.kind); std::map<std::uint32_t,const Bytes*> values;
        for (const auto& e : f.elements) {
            require(e.data.size()==size && e.index<resource_limit/4,"Invalid array element");
            require(values.emplace(e.index,&e.data).second,"Duplicate array index");
        }
        append_number(out,size,4); append_number(out,values.size(),4); append_number(out,0,4);
        for (const auto& [index,data] : values) { (void)index; out.insert(out.end(),data->begin(),data->end()); }
        std::vector<std::uint32_t> mask(values.empty()?0:values.rbegin()->first/32+1);
        for (const auto& [index,data] : values) { (void)data; mask[index/32]|=1u<<(index%32); }
        append_number(out,mask.size(),4); for (auto word : mask) append_number(out,word,4);
    }
}
}
void set_number(GameObject& object, const std::string& name, std::uint64_t value) {
    auto& f=mutable_field(object,name); require(f.kind=="INT32" || f.kind=="INT64","Expected scalar object field");
    f.present=true; f.number=value;
}
void set_null(GameObject& object, const std::string& name) {
    auto& f=mutable_field(object,name); require(f.kind!="INT32","INT32 fields cannot be null"); f.present=false; f.elements.clear(); f.text.clear();
}
Bytes encode_instance(const GameObject& object) {
    require(!object.prototype && object.prototype_id>0,"Only instances with a prototype can be written");
    auto groups=groups_for(object.type);
    std::map<std::string,const ObjectField*> fields;
    for (const auto& field : object.fields) require(fields.emplace(field.name,&field).second,"Duplicate object field");
    std::vector<std::uint32_t> masks; Bytes payload;
    unsigned matched=0;
    for (const auto& group : groups) {
        const auto& specs=field_groups.at(group); auto start=masks.size(); masks.resize(start+(specs.size()+31)/32);
        for (unsigned i=0;i<specs.size();++i) {
            auto field=fields.find(specs[i].name); if (field==fields.end()) continue;
            masks[start+i/32]|=1u<<(i%32); write_field(payload,*field->second,specs[i]); ++matched;
        }
    }
    require(matched==fields.size(),"Unknown field in object");
    Bytes out; append_number(out,119,4); append_number(out,1,2); out.insert(out.end(),6,0);
    append_number(out,object.prototype_id,4); out.insert(out.end(),12,0); append_id(out,object.id);
    append_number(out,object.type,4); append_number(out,fields.size(),2);
    for (auto mask : masks) append_number(out,mask,4);
    out.insert(out.end(),payload.begin(),payload.end());
    require(out.size()<=resource_limit,"Object exceeds resource limit");
    read_objects(out); // Validate identifiers and the resulting stream before publishing.
    return out;
}
}
