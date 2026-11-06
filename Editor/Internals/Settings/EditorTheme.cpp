#include "Editor/Internals/Settings/EditorTheme.h"
#include <algorithm>

const EditorThemeDefinition& EditorThemeInfo(EditorTheme theme) {
    const auto found = std::ranges::find(EditorThemes, theme, &EditorThemeDefinition::id);
    return found == EditorThemes.end() ? EditorThemes.front() : *found;
}
EditorTheme ParseEditorTheme(std::string_view key) {
    const auto found = std::ranges::find(EditorThemes, key, &EditorThemeDefinition::key);
    return found == EditorThemes.end() ? EditorTheme::Cabinet : found->id;
}
