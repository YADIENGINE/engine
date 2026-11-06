#pragma once
#include <array>
#include <string_view>

enum class EditorTheme { Cabinet, Slate, Paper };

struct EditorThemeDefinition {
    EditorTheme id;
    const char* key; // Stable preferences/CLI spelling; independent of enum values and display names.
    const char* title;
};

inline constexpr std::array EditorThemes{EditorThemeDefinition{EditorTheme::Cabinet, "cabinet", "Cabinet"},
                                         EditorThemeDefinition{EditorTheme::Slate, "slate", "Slate"},
                                         EditorThemeDefinition{EditorTheme::Paper, "paper", "Paper"}};

[[nodiscard]] const EditorThemeDefinition& EditorThemeInfo(EditorTheme theme);
// External text is resolved at the settings/CLI boundary; missing and unknown keys use Cabinet.
[[nodiscard]] EditorTheme ParseEditorTheme(std::string_view key);
