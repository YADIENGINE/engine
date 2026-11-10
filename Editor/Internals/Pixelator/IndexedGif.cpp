#include "Editor/Internals/Pixelator/IndexedGif.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace {
constexpr std::uint8_t OpaqueAlpha = 128; // At least this much alpha is opaque, as in the msf_gif export
constexpr int MaxCodeBits = 12;
constexpr int MaxCodes = 1 << MaxCodeBits;

std::uint32_t Key(const std::byte* pixel) {
    return static_cast<std::uint32_t>(pixel[0]) << 16 | static_cast<std::uint32_t>(pixel[1]) << 8 |
           static_cast<std::uint32_t>(pixel[2]);
}

class ByteWriter {
public:
    void Byte(int value) { _bytes.push_back(static_cast<std::byte>(value & 0xff)); }
    void Word(int value) {
        Byte(value);
        Byte(value >> 8);
    }
    void Text(std::string_view text) {
        for (const char character : text)
            Byte(static_cast<unsigned char>(character));
    }
    [[nodiscard]] std::vector<std::byte> Take() { return std::move(_bytes); }

private:
    std::vector<std::byte> _bytes;
};

// LZW codes packed least significant bit first, in data sub-blocks of at most 255 bytes.
class CodeWriter {
public:
    explicit CodeWriter(ByteWriter& out)
        : _out(out) {}
    void Write(int code, int bits) {
        _bits |= static_cast<std::uint32_t>(code) << _count;
        _count += bits;
        while (_count >= 8) {
            Push(static_cast<std::uint8_t>(_bits & 0xff));
            _bits >>= 8;
            _count -= 8;
        }
    }
    void Finish() {
        if (_count > 0) Push(static_cast<std::uint8_t>(_bits & 0xff));
        Flush();
        _out.Byte(0); // Block terminator
    }

private:
    void Push(std::uint8_t value) {
        _block[_used++] = value;
        if (_used == _block.size()) Flush();
    }
    void Flush() {
        if (_used == 0) return;
        _out.Byte(static_cast<int>(_used));
        for (std::size_t index = 0; index < _used; ++index)
            _out.Byte(_block[index]);
        _used = 0;
    }
    ByteWriter& _out;
    std::array<std::uint8_t, 255> _block{};
    std::size_t _used = 0;
    std::uint32_t _bits = 0;
    int _count = 0;
};

void WriteImageData(ByteWriter& out, std::span<const std::uint8_t> indices, int minCodeSize) {
    out.Byte(minCodeSize);
    CodeWriter codes(out);
    const int clear = 1 << minCodeSize;
    const int end = clear + 1;
    // The dictionary: (prefix code, next index) → code.
    std::unordered_map<std::uint32_t, int> table;
    int codeSize = minCodeSize + 1;
    int next = end + 1;
    codes.Write(clear, codeSize);
    int current = -1;
    for (const std::uint8_t index : indices) {
        if (current < 0) {
            current = index;
            continue;
        }
        const std::uint32_t key = static_cast<std::uint32_t>(current) << 8 | index;
        if (const auto found = table.find(key); found != table.end()) {
            current = found->second;
            continue;
        }
        codes.Write(current, codeSize);
        table.emplace(key, next++);
        if (next > (1 << codeSize) && codeSize < MaxCodeBits) ++codeSize;
        if (next == MaxCodes) {
            // A full table starts over, so long images keep compressing what they show now.
            codes.Write(clear, codeSize);
            table.clear();
            codeSize = minCodeSize + 1;
            next = end + 1;
        }
        current = index;
    }
    if (current >= 0) codes.Write(current, codeSize);
    codes.Write(end, codeSize);
    codes.Finish();
}
} // namespace

std::optional<std::vector<std::byte>> EncodeIndexedGif(std::span<const ViewportPixels> frames,
                                                       std::span<const int> delays) {
    if (frames.empty()) throw std::invalid_argument("A GIF needs at least one frame");
    if (delays.size() != frames.size()) throw std::invalid_argument("A GIF needs one delay per frame");
    const int width = frames.front().width;
    const int height = frames.front().height;
    if (width <= 0 || height <= 0 || width > 0xffff || height > 0xffff)
        throw std::invalid_argument("A GIF frame is 1 to 65535 pixels a side");
    const auto pixels = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    for (const auto& frame : frames)
        if (frame.width != width || frame.height != height || frame.rgba.size() != pixels * 4)
            throw std::invalid_argument("GIF frames must share one size");

    // Index 0 is transparent; the colours follow in the order they first appear.
    std::unordered_map<std::uint32_t, std::uint8_t> indexOf;
    std::vector<std::uint32_t> colors;
    for (const auto& frame : frames)
        for (std::size_t at = 0; at < pixels; ++at) {
            const std::byte* pixel = frame.rgba.data() + at * 4;
            if (static_cast<std::uint8_t>(pixel[3]) < OpaqueAlpha) continue;
            const auto key = Key(pixel);
            if (indexOf.contains(key)) continue;
            if (colors.size() == IndexedGifMaxColors) return std::nullopt;
            colors.push_back(key);
            indexOf.emplace(key, static_cast<std::uint8_t>(colors.size()));
        }
    int bits = 1;
    while ((std::size_t{1} << bits) < colors.size() + 1)
        ++bits;
    const int minCodeSize = std::max(2, bits);

    ByteWriter out;
    out.Text("GIF89a");
    out.Word(width);
    out.Word(height);
    out.Byte(0x80 | (bits - 1) << 4 | (bits - 1)); // Global table of 2^bits colours
    out.Byte(0);                                   // Background: the transparent index
    out.Byte(0);
    for (int entry = 0; entry < (1 << bits); ++entry) {
        const std::uint32_t color =
            entry >= 1 && entry <= static_cast<int>(colors.size()) ? colors[static_cast<std::size_t>(entry - 1)] : 0;
        out.Byte(static_cast<int>(color >> 16));
        out.Byte(static_cast<int>(color >> 8));
        out.Byte(static_cast<int>(color));
    }
    // Loop forever (NETSCAPE2.0).
    out.Byte(0x21);
    out.Byte(0xff);
    out.Byte(11);
    out.Text("NETSCAPE2.0");
    out.Byte(3);
    out.Byte(1);
    out.Word(0);
    out.Byte(0);

    std::vector<std::uint8_t> indices(pixels);
    for (std::size_t frame = 0; frame < frames.size(); ++frame) {
        // Graphic control: cleared to transparent before the next frame, so clear pixels never show the last one.
        out.Byte(0x21);
        out.Byte(0xf9);
        out.Byte(4);
        out.Byte(2 << 2 | 1);
        out.Word(std::clamp(delays[frame], 0, 0xffff));
        out.Byte(0);
        out.Byte(0);
        out.Byte(0x2c);
        out.Word(0);
        out.Word(0);
        out.Word(width);
        out.Word(height);
        out.Byte(0);
        const auto& rgba = frames[frame].rgba;
        for (std::size_t at = 0; at < pixels; ++at) {
            const std::byte* pixel = rgba.data() + at * 4;
            indices[at] = static_cast<std::uint8_t>(pixel[3]) < OpaqueAlpha ? 0 : indexOf.at(Key(pixel));
        }
        WriteImageData(out, indices, minCodeSize);
    }
    out.Byte(0x3b);
    return out.Take();
}
