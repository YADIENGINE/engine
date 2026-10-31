#pragma once
#include <cstddef>
#include <string_view>

// Decodes the next UTF-8 codepoint at 'text[at]' and advances 'at'; malformed bytes decode as U+FFFD.
[[nodiscard]] char32_t NextCodepoint(std::string_view text, std::size_t& at);
