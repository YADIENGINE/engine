#include "ToolUi/ToolPixel.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include "ToolUi/ToolTheme.h"

namespace {
constexpr int GlyphColumns = 5;
constexpr int GlyphRows = 7;
constexpr int Advance = GlyphColumns + 1;
constexpr float TileCellShare = 0.085f; // Of the tile's side
constexpr float StateCellSize = 7.0f;
constexpr float ActivityCell = 3.0f;
constexpr float ActivityGap = 1.0f;
constexpr double ActivityStepSeconds = 0.11;
constexpr float ActivityTrailBright = 0.45f;
constexpr float ActivityDim = 0.14f;
// One row per line, bit 4 is the leftmost column. Scripts/brand.py in the launcher draws the icon from the same font.
using Glyph = std::array<std::uint8_t, GlyphRows>;
constexpr std::array Letters{
    Glyph{0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, // A
    Glyph{0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}, // B
    Glyph{0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}, // C
    Glyph{0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e}, // D
    Glyph{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}, // E
    Glyph{0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}, // F
    Glyph{0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e}, // G
    Glyph{0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, // H
    Glyph{0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e}, // I
    Glyph{0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0c}, // J
    Glyph{0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}, // K
    Glyph{0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}, // L
    Glyph{0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}, // M
    Glyph{0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, // N
    Glyph{0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, // O
    Glyph{0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}, // P
    Glyph{0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}, // Q
    Glyph{0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}, // R
    Glyph{0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}, // S
    Glyph{0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}, // T
    Glyph{0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}, // U
    Glyph{0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}, // V
    Glyph{0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}, // W
    Glyph{0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}, // X
    Glyph{0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}, // Y
    Glyph{0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}  // Z
};
constexpr std::array Digits{
    Glyph{0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}, // 0
    Glyph{0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}, // 1
    Glyph{0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}, // 2
    Glyph{0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e}, // 3
    Glyph{0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}, // 4
    Glyph{0x1f, 0x10, 0x1e, 0x01, 0x01, 0x11, 0x0e}, // 5
    Glyph{0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}, // 6
    Glyph{0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}, // 7
    Glyph{0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}, // 8
    Glyph{0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}  // 9
};
constexpr Glyph Dash{0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00};
constexpr Glyph Blank{};

const Glyph& GlyphOf(char character) {
    if (character >= 'a' && character <= 'z') return Letters[static_cast<std::size_t>(character - 'a')];
    if (character >= 'A' && character <= 'Z') return Letters[static_cast<std::size_t>(character - 'A')];
    if (character >= '0' && character <= '9') return Digits[static_cast<std::size_t>(character - '0')];
    return character == ' ' ? Blank : Dash;
}

ImVec4 Rgb(int red, int green, int blue) {
    return {static_cast<float>(red) / 255.0f, static_cast<float>(green) / 255.0f, static_cast<float>(blue) / 255.0f,
            1.0f};
}

// Decodes one UTF-8 sequence's base letter for the Turkish alphabet; 0 for anything else multi-byte.
char FoldTurkish(unsigned char lead, unsigned char next) {
    if (lead == 0xc3) {
        if (next == 0x87 || next == 0xa7) return 'C';
        if (next == 0x96 || next == 0xb6) return 'O';
        if (next == 0x9c || next == 0xbc) return 'U';
    }
    if (lead == 0xc4) {
        if (next == 0x9e || next == 0x9f) return 'G';
        if (next == 0xb0 || next == 0xb1) return 'I';
    }
    if (lead == 0xc5 && (next == 0x9e || next == 0x9f)) return 'S';
    return 0;
}
} // namespace

float ToolPixelCell(float logicalCell) { return std::max(1.0f, std::round(logicalCell * ToolUiScale())); }

ImVec2 ToolPixelTextSize(std::string_view text, float cell) {
    if (text.empty()) return {0, 0};
    return {(static_cast<float>(text.size()) * Advance - 1) * cell, GlyphRows * cell};
}

void ToolDrawPixelText(ImDrawList& draw, ImVec2 position, std::string_view text, float cell, ImU32 color) {
    const ImVec2 start{std::floor(position.x), std::floor(position.y)};
    float x = start.x;
    for (const char character : text) {
        const auto& glyph = GlyphOf(character);
        for (int row = 0; row < GlyphRows; ++row)
            for (int column = 0; column < GlyphColumns; ++column)
                if (glyph[static_cast<std::size_t>(row)] & (1u << (GlyphColumns - 1 - column))) {
                    const ImVec2 from{x + static_cast<float>(column) * cell, start.y + static_cast<float>(row) * cell};
                    draw.AddRectFilled(from, {from.x + cell, from.y + cell}, color);
                }
        x += Advance * cell;
    }
}

void ToolPixelText(std::string_view text, float logicalCell, ImVec4 color, float height) {
    const float cell = ToolPixelCell(logicalCell);
    const auto size = ToolPixelTextSize(text, cell);
    const auto start = ImGui::GetCursorScreenPos();
    const float itemHeight = std::max(size.y, height);
    ImGui::Dummy({size.x, itemHeight});
    ToolDrawPixelText(*ImGui::GetWindowDrawList(), {start.x, start.y + (itemHeight - size.y) * 0.5f}, text, cell,
                      ImGui::GetColorU32(color));
}

char ToolPixelInitial(std::string_view name) {
    for (std::size_t index = 0; index < name.size(); ++index) {
        const auto byte = static_cast<unsigned char>(name[index]);
        if (byte < 0x80) {
            if (byte >= 'a' && byte <= 'z') return static_cast<char>(byte - 'a' + 'A');
            if ((byte >= 'A' && byte <= 'Z') || (byte >= '0' && byte <= '9')) return static_cast<char>(byte);
            continue;
        }
        if (index + 1 < name.size())
            if (const char folded = FoldTurkish(byte, static_cast<unsigned char>(name[index + 1]))) return folded;
    }
    return '-';
}

ToolTileColors ToolTileColorsFor(std::string_view name) {
    const std::array pairs{
        ToolTileColors{Rgb(0x24, 0x35, 0x2a), Rgb(0x8f, 0xc7, 0x9a)}, // Green
        ToolTileColors{Rgb(0x2d, 0x2a, 0x40), Rgb(0xaa, 0xa4, 0xe2)}, // Violet
        ToolTileColors{Rgb(0x3a, 0x2c, 0x20), Rgb(0xf4, 0xaf, 0x65)}, // Amber
        ToolTileColors{Rgb(0x3a, 0x23, 0x28), Rgb(0xe8, 0x85, 0x8f)}, // Rose
        ToolTileColors{Rgb(0x1f, 0x33, 0x3a), Rgb(0x83, 0xc4, 0xd6)}, // Teal
        ToolTileColors{Rgb(0x33, 0x33, 0x20), Rgb(0xd0, 0xcb, 0x7c)}  // Olive
    };
    std::uint32_t hash = 2166136261u; // FNV-1a: stable across runs and machines
    for (const char character : name)
        hash = (hash ^ static_cast<unsigned char>(character)) * 16777619u;
    return pairs[hash % pairs.size()];
}

void ToolDrawPixelTile(ImDrawList& draw, ImVec2 position, std::string_view name, float side) {
    const ImVec2 start{std::floor(position.x), std::floor(position.y)};
    const auto colors = ToolTileColorsFor(name);
    draw.AddRectFilled(start, {start.x + side, start.y + side}, ImGui::GetColorU32(colors.background),
                       ToolMetrics::Radius * ToolUiScale());
    const float cell = std::max(1.0f, std::floor(side * TileCellShare));
    const char initial = ToolPixelInitial(name);
    const std::string_view letter(&initial, 1);
    const auto extent = ToolPixelTextSize(letter, cell);
    ToolDrawPixelText(draw, {start.x + (side - extent.x) * 0.5f, start.y + (side - extent.y) * 0.5f}, letter, cell,
                      ImGui::GetColorU32(colors.foreground));
}

void ToolPixelTile(std::string_view name, float size) {
    const float side = std::round(size * ToolUiScale());
    const auto start = ImGui::GetCursorScreenPos();
    ImGui::Dummy({side, side});
    ToolDrawPixelTile(*ImGui::GetWindowDrawList(), start, name, side);
}

void ToolStateCell(ImVec4 color) {
    const float side = ToolPixelCell(StateCellSize);
    const float height = ImGui::GetTextLineHeight();
    const auto start = ImGui::GetCursorScreenPos();
    ImGui::Dummy({side, height});
    const ImVec2 from{std::floor(start.x), std::floor(start.y + (height - side) * 0.5f)};
    ImGui::GetWindowDrawList()->AddRectFilled(from, {from.x + side, from.y + side}, ImGui::GetColorU32(color));
}

float ToolActivitySide() { return ToolPixelCell(ActivityCell) * 3 + ToolPixelCell(ActivityGap) * 2; }
void ToolDrawActivity(ImDrawList& draw, ImVec2 position, ImVec4 color, bool running) {
    // The square's edge, clockwise from the top left.
    static constexpr std::array<ImVec2, 8> Ring{ImVec2{0, 0}, ImVec2{1, 0}, ImVec2{2, 0}, ImVec2{2, 1},
                                                ImVec2{2, 2}, ImVec2{1, 2}, ImVec2{0, 2}, ImVec2{0, 1}};
    const float cell = ToolPixelCell(ActivityCell);
    const float gap = ToolPixelCell(ActivityGap);
    const ImVec2 origin{std::floor(position.x), std::floor(position.y)};
    const auto head = static_cast<std::size_t>(ImGui::GetTime() / ActivityStepSeconds) % Ring.size();
    const float alpha = color.w;
    for (std::size_t index = 0; index < Ring.size(); ++index) {
        const std::size_t behind = (head + Ring.size() - index) % Ring.size();
        color.w = alpha * (!running || behind == 0 ? 1.0f : behind <= 2 ? ActivityTrailBright : ActivityDim);
        const ImVec2 from{origin.x + Ring[index].x * (cell + gap), origin.y + Ring[index].y * (cell + gap)};
        draw.AddRectFilled(from, {from.x + cell, from.y + cell}, ImGui::GetColorU32(color));
    }
}
void ToolActivity() {
    const float side = ToolActivitySide();
    const float height = std::max(ImGui::GetTextLineHeight(), side);
    const auto start = ImGui::GetCursorScreenPos();
    ImGui::Dummy({side, height});
    ToolDrawActivity(*ImGui::GetWindowDrawList(), {start.x, start.y + (height - side) * 0.5f},
                     ToolToneColor(ToolTone::Accent), true);
}
