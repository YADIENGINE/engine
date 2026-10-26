#pragma once
#include <string>
#include <string_view>

// One RFC 1952 gzip member, including CRC32 and uncompressed byte count. Write-only.
class GzipCodec final {
public:
    [[nodiscard]] static std::string Encode(std::string_view text);
};
