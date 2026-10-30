#include "Tests/Support/GzipInflate.h"
#include <catch2/catch_test_macros.hpp>

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define SINFL_IMPLEMENTATION
#include "Tests/Support/third_party/sinfl.h"
#undef SINFL_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(pop)
#endif

std::uint32_t LittleEndian(std::string_view bytes) {
    REQUIRE(bytes.size() == 4);
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bytes.size(); ++i)
        value |= static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i])) << (8 * i);
    return value;
}
std::string InflateGzip(const std::string& bytes) {
    REQUIRE(bytes.size() >= 18);
    REQUIRE(static_cast<unsigned char>(bytes[0]) == 0x1f);
    REQUIRE(static_cast<unsigned char>(bytes[1]) == 0x8b);
    REQUIRE(bytes[2] == 8);
    REQUIRE(bytes[3] == 0);
    const auto size = LittleEndian(std::string_view(bytes).substr(bytes.size() - 4));
    std::string text(size + 1, '\0');
    const int decoded =
        sinflate(text.data(), static_cast<int>(text.size()), bytes.data() + 10, static_cast<int>(bytes.size() - 18));
    REQUIRE(decoded == static_cast<int>(size));
    text.resize(size);
    return text;
}
std::uint32_t ReferenceCrc32(std::string_view text) {
    std::uint32_t crc = 0xffffffffu;
    for (unsigned char ch : text) {
        crc ^= ch;
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320u : 0u);
    }
    return ~crc;
}
