#include "worlded/assets.hpp"
#include "binary.hpp"
#include <algorithm>

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
    headers_offset_ = in.pos;
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
Bytes Art::indexed(unsigned rotation, unsigned index) const {
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
    return indexed;
}
Bytes Art::rgba(unsigned rotation, unsigned index, unsigned palette) const {
    if (!palette_available(palette)) throw std::runtime_error("ART palette is not available");
    auto indices = indexed(rotation, index);
    const auto pixels = indices.size();
    Bytes result(pixels * 4);
    for (std::size_t i = 0; i < pixels; ++i) {
        auto color = palettes_[palette][indices[i]];
        result[4*i] = (color >> 16) & 255;
        result[4*i+1] = (color >> 8) & 255;
        result[4*i+2] = color & 255;
        result[4*i+3] = indices[i] ? 255 : 0;
    }
    return result;
}

namespace {
void put32(Bytes& data, std::size_t at, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) data.at(at+i)=std::uint8_t(value>>(i*8));
}
void append32(Bytes& data, std::uint32_t value) {
    auto at=data.size(); data.resize(at+4); put32(data,at,value);
}
Bytes compress_art(const Bytes& pixels) {
    Bytes out;
    for (std::size_t i=0;i<pixels.size();) {
        std::size_t run=1;
        while (run<127 && i+run<pixels.size() && pixels[i+run]==pixels[i]) ++run;
        if (run>=3) { out.push_back(std::uint8_t(run)); out.push_back(pixels[i]); i+=run; }
        else {
            auto start=i;
            do {
                ++i;
                if (i+2<pixels.size() && pixels[i]==pixels[i+1] && pixels[i]==pixels[i+2]) break;
            } while (i<pixels.size() && i-start<127);
            out.push_back(std::uint8_t(128|(i-start)));
            out.insert(out.end(),pixels.begin()+start,pixels.begin()+i);
        }
    }
    return out.size()<pixels.size()?out:pixels;
}
}
unsigned Art::flags() const { return BinaryReader(data_).u32(); }
std::int32_t Art::action_frame() const { return std::int32_t(BinaryReader(data_,28).u32()); }
const Art::Palette& Art::palette(unsigned slot) const {
    if (!palette_available(slot)) throw std::runtime_error("ART palette is not available");
    return palettes_[slot];
}
void Art::set_palette(unsigned slot, const Palette& colors) {
    if (slot>=4) throw std::runtime_error("ART palette out of range");
    std::size_t at=132;
    for (unsigned p=0;p<slot;++p) if (present_[p]) at+=1024;
    if (!present_[slot]) {
        data_.insert(data_.begin()+at,1024,0); headers_offset_+=1024;
        for (auto& f:frame_data_) f.offset+=1024;
        present_[slot]=true; put32(data_,12+4*slot,1);
    }
    palettes_[slot]=colors;
    for (auto color:colors) { put32(data_,at,color); at+=4; }
}
void Art::set_fps(unsigned value) {
    if (!value || value>1000) throw std::runtime_error("FPS must be between 1 and 1000");
    fps_=value; put32(data_,4,value);
}
void Art::set_action_frame(std::int32_t value) {
    if (value < -1 || value>=std::int32_t(frames_)) throw std::runtime_error("Action frame out of range");
    put32(data_,28,std::uint32_t(value));
}
void Art::set_anchor(unsigned rotation, unsigned index, std::int32_t x, std::int32_t y) {
    frame(rotation,index); auto n=rotation*frames_+index; auto& f=frame_data_[n];
    f.hot_x=x; f.hot_y=y; put32(data_,headers_offset_+28*n+12,std::uint32_t(x));
    put32(data_,headers_offset_+28*n+16,std::uint32_t(y));
}
void Art::set_offset(unsigned rotation, unsigned index, std::int32_t x, std::int32_t y) {
    frame(rotation,index); auto n=rotation*frames_+index; auto& f=frame_data_[n];
    f.offset_x=x; f.offset_y=y; put32(data_,headers_offset_+28*n+20,std::uint32_t(x));
    put32(data_,headers_offset_+28*n+24,std::uint32_t(y));
}
void Art::validate() const {
    for (unsigned r=0;r<rotations_;++r) for (unsigned f=0;f<frames_;++f) indexed(r,f);
}
Art Art::create(unsigned rotations, unsigned fps, const Palette& palette,
        const std::vector<ArtFrame>& frames, const std::vector<Bytes>& pixels) {
    if ((rotations!=1 && rotations!=8) || frames.empty() || frames.size()%rotations
        || frames.size()/rotations>4096 || frames.size()!=pixels.size() || !fps || fps>1000)
        throw std::runtime_error("Invalid ART animation layout");
    Bytes data(132,0); put32(data,0,rotations==1?1:0); put32(data,4,fps);
    put32(data,8,8); put32(data,12,1); put32(data,32,unsigned(frames.size()/rotations));
    for (auto color:palette) append32(data,color);
    std::vector<Bytes> encoded;
    std::array<std::uint32_t,8> sizes{};
    std::uint64_t total=0;
    for (std::size_t i=0;i<frames.size();++i) {
        const auto& f=frames[i]; const auto size=std::uint64_t(f.width)*f.height;
        if (!f.width || !f.height || f.width>16384 || f.height>16384 || size>resource_limit/4 || size!=pixels[i].size())
            throw std::runtime_error("Invalid ART pixel dimensions");
        total+=size;
        if (total>resource_limit) throw std::runtime_error("ART animation exceeds resource limit");
        encoded.push_back(compress_art(pixels[i]));
        sizes[i/(frames.size()/rotations)]+=unsigned(encoded.back().size()); // Stored bytes per rotation, as in original ART files.
        append32(data,f.width); append32(data,f.height); append32(data,unsigned(encoded.back().size()));
        append32(data,std::uint32_t(f.hot_x)); append32(data,std::uint32_t(f.hot_y));
        append32(data,std::uint32_t(f.offset_x)); append32(data,std::uint32_t(f.offset_y));
    }
    for (unsigned r=0;r<rotations;++r) put32(data,68+4*r,sizes[r]);
    if (rotations==1) for (unsigned r=1;r<8;++r) put32(data,68+4*r,sizes[0]);
    for (const auto& bytes:encoded) data.insert(data.end(),bytes.begin(),bytes.end());
    return Art(std::move(data));
}
}
