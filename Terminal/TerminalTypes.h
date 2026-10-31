#pragma once
#include <array>
#include <compare>
#include <cstdint>

// Renkler 0xRRGGBB. İlk 16'sı ANSI paleti (0-7 normal, 8-15 parlak).
struct TerminalPalette {
    std::uint32_t foreground = 0xCCCCCC;
    std::uint32_t background = 0x1E1E1E;
    std::array<std::uint32_t, 16> ansi{0x000000, 0xCD3131, 0x0DBC79, 0xE5E510, 0x2472C8, 0xBC3FBC, 0x11A8CD, 0xE5E5E5,
                                       0x666666, 0xF14C4C, 0x23D18B, 0xF5F543, 0x3B8EEA, 0xD670D6, 0x29B8DB, 0xFFFFFF};
};

// Ins / Del: Insert / Delete. İşlev tuşları (F1..F12) ayrı: SendFunctionKey.
enum class TerminalKey { Enter, Tab, Backspace, Escape, Up, Down, Left, Right, Ins, Del, Home, End, PageUp, PageDown };

struct TerminalModifiers {
    bool shift = false;
    bool alt = false;
    bool ctrl = false;
};

// Block / Underline / Bar: programın istediği imleç biçimi.
enum class TerminalCursorShape { Block, Underline, Bar };

struct TerminalCursor {
    int row = 0;
    int column = 0;
    bool visible = true;
    TerminalCursorShape shape = TerminalCursorShape::Block;
};

// Hücre sınırı: column satırın sonunu göstermek için Columns() olabilir.
struct TerminalPosition {
    int row = 0;
    int column = 0;
    [[nodiscard]] auto operator<=>(const TerminalPosition&) const = default;
};

// Bir ekran hücresi. text en çok bir taban karakter + birleşen işaretler (UTF-32, 0 ile biter); geniş karakterin
// ikinci hücresi continuation'dır ve çizilmez. Renkler ters (reverse) uygulanmış haldedir.
struct TerminalCell {
    std::array<char32_t, 7> text{};
    std::uint32_t foreground = 0;
    std::uint32_t background = 0;
    bool defaultBackground = true;
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool strike = false;
    bool continuation = false;
    std::uint8_t width = 1;
};
