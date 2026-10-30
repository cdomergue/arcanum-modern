#include "worlded/assets.hpp"
#include "binary.hpp"

namespace worlded {
Art::Art(Bytes data) : data_(std::move(data)) {
    BinaryReader in(data_);
    rotations_ = in.u32() & 1 ? 1 : 8;
    fps_ = in.u32();
    if (in.u32() != 8) throw std::runtime_error("Only indexed 8-bit ART images are supported");
    for (auto& present : present_) present = in.u32() != 0;
    in.u32(); // Action frame.
    frames_ = in.u32();
    if (!frames_ || frames_ > 4096) throw std::runtime_error("Invalid ART frame count");
    in.skip(96); // 32-bit pointer tables and stored per-rotation sizes.
    for (unsigned p = 0; p < 4; ++p) if (present_[p])
        for (auto& color : palettes_[p]) color = in.u32();
    if (!present_[0]) throw std::runtime_error("ART has no default palette");
    for (unsigned i = 0; i < rotations_ * frames_; ++i) {
        ArtFrame f{};
        f.width = in.u32(); f.height = in.u32(); f.encoded_size = in.u32();
        f.hot_x = static_cast<std::int32_t>(in.u32()); f.hot_y = static_cast<std::int32_t>(in.u32());
        f.offset_x = static_cast<std::int32_t>(in.u32()); f.offset_y = static_cast<std::int32_t>(in.u32());
        if (!f.width || !f.height || f.width > 16384 || f.height > 16384
            || std::uint64_t(f.width) * f.height > resource_limit / 4)
            throw std::runtime_error("Invalid or excessive ART frame dimensions");
        frame_data_.push_back(f);
    }
    for (auto& frame : frame_data_) { frame.offset = in.pos; in.skip(frame.encoded_size); }
    // Original assets can retain unused trailing bytes. CE ignores them too;
    // bounds and exact RLE expansion are checked for every requested frame.
}
bool Art::palette_available(unsigned palette) const { return palette < 4 && present_[palette]; }
const ArtFrame& Art::frame(unsigned rotation, unsigned index) const {
    if (rotation >= rotations_ || index >= frames_) throw std::runtime_error("ART frame out of range");
    return frame_data_[rotation * frames_ + index];
}
Bytes Art::rgba(unsigned rotation, unsigned index, unsigned palette) const {
    if (!palette_available(palette)) throw std::runtime_error("ART palette is not available");
    const auto& f = frame(rotation, index);
    const std::size_t pixels = std::size_t(f.width) * f.height;
    Bytes indexed;
    indexed.reserve(pixels);
    BinaryReader in(data_, f.offset, f.offset + f.encoded_size);
    if (f.encoded_size == pixels) indexed = in.bytes(pixels);
    else if (f.encoded_size == 0) indexed.resize(pixels, 0);
    else {
        while (in.pos < in.end) {
            auto code = in.u8();
            unsigned length = code & 127;
            if (!length || length > pixels - indexed.size()) throw std::runtime_error("Invalid ART RLE run");
            if (code & 128) {
                auto bytes = in.bytes(length); indexed.insert(indexed.end(), bytes.begin(), bytes.end());
            } else indexed.insert(indexed.end(), length, static_cast<std::uint8_t>(in.u8()));
        }
        if (indexed.size() != pixels) throw std::runtime_error("Incomplete ART RLE frame");
    }
    Bytes result(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
        auto color = palettes_[palette][indexed[i]];
        result[4*i] = (color >> 16) & 255;
        result[4*i+1] = (color >> 8) & 255;
        result[4*i+2] = color & 255;
        result[4*i+3] = indexed[i] ? 255 : 0;
    }
    return result;
}
}
