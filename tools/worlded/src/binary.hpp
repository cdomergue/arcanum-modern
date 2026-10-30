#pragma once
#include "worlded/formats.hpp"
#include <stdexcept>

namespace worlded {
class BinaryReader {
public:
    const Bytes& data;
    std::size_t pos, end;
    BinaryReader(const Bytes& bytes, std::size_t start = 0, std::size_t limit = 0)
        : data(bytes), pos(start), end(limit ? limit : bytes.size()) {
        if (pos > end || end > data.size()) throw std::runtime_error("Invalid binary range");
    }
    void need(std::size_t count) const {
        if (count > end - pos) throw std::runtime_error("Truncated binary at byte " + std::to_string(pos));
    }
    void skip(std::size_t count) { need(count); pos += count; }
    unsigned u8() { need(1); return data[pos++]; }
    unsigned u16() { auto lo = u8(); return lo | (u8() << 8); }
    std::uint32_t u32() { auto lo = u16(); return lo | (std::uint32_t(u16()) << 16); }
    std::uint64_t u64() { auto lo = u32(); return lo | (std::uint64_t(u32()) << 32); }
    Bytes bytes(std::size_t count) {
        need(count); auto begin = pos; pos += count;
        return Bytes(data.begin() + begin, data.begin() + pos);
    }
    void records(std::size_t count, std::size_t size) {
        if (!size || count > (end - pos) / size) throw std::runtime_error("Invalid record count");
        skip(count * size);
    }
};
}
