#include "Terminal/Internals/TerminalText.h"
#include <cstdint>

namespace {
void AppendUtf8(std::string& text, char32_t character) {
    const auto c = static_cast<std::uint32_t>(character);
    if (c < 0x80) {
        text += static_cast<char>(c);
    } else if (c < 0x800) {
        text += static_cast<char>(0xC0 | (c >> 6));
        text += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
        text += static_cast<char>(0xE0 | (c >> 12));
        text += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (c & 0x3F));
    } else {
        text += static_cast<char>(0xF0 | (c >> 18));
        text += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (c & 0x3F));
    }
}
} // namespace

std::string TerminalCellText(const TerminalCell& cell) {
    std::string text;
    for (const char32_t character : cell.text) {
        if (!character) break;
        AppendUtf8(text, character);
    }
    return text;
}
