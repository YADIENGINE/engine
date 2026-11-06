#pragma once
#include <span>
#include "Editor/Internals/Settings/EditorTheme.h"
#include "ToolUi/ToolColorPalette.h"

struct EditorPalettePreset {
    EditorTheme id;
    const char* key;
    const char* title;
    ToolColorPalette colors;
};

// Metadata comes from the UI-independent EditorThemes table; colors stay in the UI layer.
[[nodiscard]] std::span<const EditorPalettePreset> EditorPalettePresets();
[[nodiscard]] const EditorPalettePreset& ResolveEditorPalette(EditorTheme theme);
