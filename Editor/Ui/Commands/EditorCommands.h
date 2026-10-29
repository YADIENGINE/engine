#pragma once
#include <array>
#include <imgui.h>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Ui/Style/EditorTheme.h"

struct EditorCommandInfo {
    EditorCommand command;
    const char* label;
    EditorGlyph glyph;
    ImGuiKeyChord shortcut;
    const char* shortcutLabel;
};

[[nodiscard]] const std::array<EditorCommandInfo, static_cast<std::size_t>(EditorCommand::Count)>& EditorCommands();
[[nodiscard]] const char* EditorCommandLabel(EditorCommand command, const EditorSnapshot& snapshot);
[[nodiscard]] EditorCommand EditorPrimaryCommand(const EditorSnapshot& snapshot);
void EditorCommandMenuItem(EditorCommand command, const EditorSnapshot& snapshot, const EditorUiActions& actions);
bool EditorCommandButton(EditorCommand command, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         bool showLabel = false);
