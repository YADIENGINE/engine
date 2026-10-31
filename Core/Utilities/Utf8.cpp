#include "Utilities/Utf8.h"

namespace {
constexpr char32_t Replacement = 0xFFFD;
} // namespace

char32_t NextCodepoint(std::string_view text, std::size_t& at) {
    const auto byte = [&](std::size_t i) { return static_cast<unsigned char>(text[i]); };
    const unsigned char lead = byte(at);
    int length = 0;
    char32_t codepoint = 0;
    if (lead < 0x80) {
        ++at;
        return lead;
    }
    if ((lead & 0xE0) == 0xC0) {
        length = 2;
        codepoint = lead & 0x1Fu;
    } else if ((lead & 0xF0) == 0xE0) {
        length = 3;
        codepoint = lead & 0x0Fu;
    } else if ((lead & 0xF8) == 0xF0) {
        length = 4;
        codepoint = lead & 0x07u;
    } else {
        ++at;
        return Replacement;
    }
    if (at + static_cast<std::size_t>(length) > text.size()) {
        at = text.size();
        return Replacement;
    }
    for (int i = 1; i < length; ++i) {
        const unsigned char next = byte(at + static_cast<std::size_t>(i));
        if ((next & 0xC0) != 0x80) {
            at += static_cast<std::size_t>(i);
            return Replacement;
        }
        codepoint = (codepoint << 6) | (next & 0x3Fu);
    }
    at += static_cast<std::size_t>(length);
    return codepoint;
}
