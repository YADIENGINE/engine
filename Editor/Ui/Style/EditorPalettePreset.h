#pragma once
#include <span>
#include <string_view>
#include "ToolUi/ToolColorPalette.h"

struct EditorPalettePreset {
    const char* id;
    const char* title;
    ToolColorPalette colors;
};

// A single catalog feeds the menu, command palette and saved-id resolution.
[[nodiscard]] std::span<const EditorPalettePreset> EditorPalettePresets();
// Missing and unknown ids resolve to Cabinet without discarding other preferences.
[[nodiscard]] const EditorPalettePreset& ResolveEditorPalette(std::string_view id);
