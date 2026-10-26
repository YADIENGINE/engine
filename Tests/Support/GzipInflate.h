#pragma once
#include <cstdint>
#include <string>
#include <string_view>

// Test-only reader for one gzip member; checks the header and the trailer's byte count.
[[nodiscard]] std::string InflateGzip(const std::string& bytes);
[[nodiscard]] std::uint32_t LittleEndian(std::string_view bytes);
// Independent bitwise CRC32 reference for checking stored trailers.
[[nodiscard]] std::uint32_t ReferenceCrc32(std::string_view text);
