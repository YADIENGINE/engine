#pragma once
#include <imgui.h>
#include <string_view>

// The tools' identity: one 5x7 pixel font (the same as the launcher's icon, Scripts/brand.py there) and the square cell
// it is made of. The product name, project tiles, state marks and the activity indicator are all drawn from it.

// Whole-pixel cell size for a logical one at the current interface scale.
[[nodiscard]] float ToolPixelCell(float logicalCell);
[[nodiscard]] ImVec2 ToolPixelTextSize(std::string_view text, float cell);
// Latin letters and digits, upper-cased; anything else draws a dash. `cell` is in real pixels (ToolPixelCell).
void ToolDrawPixelText(ImDrawList& draw, ImVec2 position, std::string_view text, float cell, ImU32 color);
// As an item, vertically centered in `height` when given. `logicalCell` scales with the interface.
void ToolPixelText(std::string_view text, float logicalCell, ImVec4 color, float height = 0);

// What a project's tile shows: the first letter or digit of its name, Turkish letters folded to their base; '-' when
// the name has neither.
[[nodiscard]] char ToolPixelInitial(std::string_view name);
struct ToolTileColors {
    ImVec4 background;
    ImVec4 foreground;
};
// Muted pairs chosen by the name, so a project keeps its color everywhere.
[[nodiscard]] ToolTileColors ToolTileColorsFor(std::string_view name);
// A square tile with the initial of `name`: drawn at `position` with `side` in real pixels, or as an item with a
// logical `size`.
void ToolDrawPixelTile(ImDrawList& draw, ImVec2 position, std::string_view name, float side);
void ToolPixelTile(std::string_view name, float size);

// A state mark as an item: one square cell, centered in the current text line.
void ToolStateCell(ImVec4 color);
// "Work is running": lit cells walking around a 3x3 square, as an item centered in the current text line.
void ToolActivity();
