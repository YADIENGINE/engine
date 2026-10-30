#include "Modules/JsonIO/Internals/GzipCodec.h"
#include <array>
#include <cstdint>
#include <limits>
#include <memory>

#ifdef _MSC_VER
#include <intrin.h>
#pragma warning(push, 0)
#endif
// Keep the private compressor symbols distinct from raylib's copy of sdefl.
#define sdeflate engine_sdeflate
#define zsdeflate engine_zsdeflate
#define sdefl_bound engine_sdefl_bound
#define SDEFL_IMPLEMENTATION
#include "Modules/JsonIO/Internals/sdefl.h"
#undef SDEFL_IMPLEMENTATION
#undef sdefl_bound
#undef zsdeflate
#undef sdeflate
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include "Modules/JsonIO/JsonIO.hpp"

namespace {
std::uint32_t Crc32(std::string_view text) {
    static constexpr auto table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::size_t i = 0; i < values.size(); ++i) {
            std::uint32_t value = static_cast<std::uint32_t>(i);
            for (int bit = 0; bit < 8; ++bit)
                value = (value >> 1) ^ ((value & 1) ? 0xedb88320u : 0u);
            values[i] = value;
        }
        return values;
    }();
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char ch : text)
        crc = table[(crc ^ ch) & 0xffu] ^ (crc >> 8);
    return ~crc;
}
void AppendLittleEndian(std::string& output, std::uint32_t value) {
    for (int byte = 0; byte < 4; ++byte) {
        output += static_cast<char>(value & 0xffu);
        value >>= 8;
    }
}
} // namespace

std::string GzipCodec::Encode(std::string_view text) {
    // sdefl uses int offsets and adds a block length while scanning the input.
    constexpr auto MaxInput = static_cast<std::size_t>(std::numeric_limits<int>::max() - SDEFL_BLK_MAX);
    if (text.size() > MaxInput)
        throw JsonIO::Error(JsonIO::ErrorCode::LimitExceeded, "JSON exceeds the gzip input limit");
    constexpr char Header[] = "\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff";
    constexpr std::size_t HeaderSize = sizeof(Header) - 1;
    std::string output(Header, HeaderSize);
    if (text.empty()) {
        // sdefl omits empty blocks; gzip still needs a final DEFLATE block.
        output.append("\x03\x00", 2);
    } else {
        const int inputSize = static_cast<int>(text.size());
        const int bound = engine_sdefl_bound(inputSize);
        output.resize(HeaderSize + static_cast<std::size_t>(bound));
        // The compressor state is too large for a Windows stack frame.
        auto state = std::make_unique<sdefl>();
        const int size = engine_sdeflate(state.get(), output.data() + HeaderSize, text.data(), inputSize, 6);
        if (size <= 0 || size > bound) throw JsonIO::Error(JsonIO::ErrorCode::Io, "Could not compress JSON");
        output.resize(HeaderSize + static_cast<std::size_t>(size));
    }
    AppendLittleEndian(output, Crc32(text));
    AppendLittleEndian(output, static_cast<std::uint32_t>(text.size()));
    return output;
}
