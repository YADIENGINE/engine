#include "Editor/Ui/Commands/EditorCommands.h"
#include <string>
#include "Editor/Application/EditorCommandState.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
const EditorCommandInfo& CommandInfo(EditorCommand command) {
    return EditorCommands()[static_cast<std::size_t>(command)];
}
const char* CommandShortcut(EditorCommand command, const EditorSnapshot& snapshot) {
    if (command == EditorPrimaryCommand(snapshot)) return "F5";
    return CommandInfo(command).shortcutLabel;
}
EditorGlyph CommandGlyph(EditorCommand command, const EditorSnapshot& snapshot) {
    if (command == EditorCommand::Pause && snapshot.paused) return EditorGlyph::Play;
    return CommandInfo(command).glyph;
}
ImVec4 CommandColor(EditorCommand command, const EditorSnapshot& snapshot) {
    switch (command) {
    case EditorCommand::Open   : return EditorToneColor(EditorTone::Accent);
    case EditorCommand::Stop   : return EditorToneColor(EditorTone::Error);
    case EditorCommand::Pause  : return EditorToneColor(snapshot.paused ? EditorTone::Success : EditorTone::Warning);
    case EditorCommand::Refresh: return EditorToneColor(EditorTone::Information);
    case EditorCommand::Cancel : return EditorToneColor(EditorTone::Warning);
    case EditorCommand::Reset  :
    case EditorCommand::Step   :
    case EditorCommand::Count  : return ImGui::GetStyleColorVec4(ImGuiCol_Text);
    }
    return {};
}
std::string CommandTooltip(EditorCommand command, const EditorSnapshot& snapshot) {
    std::string text = EditorCommandLabel(command, snapshot);
    if (const auto* shortcut = CommandShortcut(command, snapshot); shortcut && *shortcut) {
        text += "  (";
        text += shortcut;
        text += ')';
    }
    if (!snapshot.capabilities.Can(command)) {
        const auto* reason = snapshot.capabilities.Reason(command);
        if (*reason) text += std::string("\n") + reason;
    }
    return text;
}
} // namespace

const std::array<EditorCommandInfo, static_cast<std::size_t>(EditorCommand::Count)>& EditorCommands() {
    static constexpr std::array commands{
        EditorCommandInfo{EditorCommand::Open, "Open", EditorGlyph::Play, 0, nullptr},
        EditorCommandInfo{EditorCommand::Stop, "Stop", EditorGlyph::Stop, 0, nullptr},
        EditorCommandInfo{EditorCommand::Pause, "Pause", EditorGlyph::Pause, ImGuiKey_F6, "F6"},
        EditorCommandInfo{EditorCommand::Reset, "Reset", EditorGlyph::Reset, ImGuiMod_Ctrl | ImGuiKey_R, "Ctrl+R"},
        EditorCommandInfo{EditorCommand::Refresh, "Build & Refresh", EditorGlyph::Build,
                          ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B, "Ctrl+Shift+B"},
        EditorCommandInfo{EditorCommand::Cancel, "Cancel operation", EditorGlyph::Cancel, 0, nullptr},
        EditorCommandInfo{EditorCommand::Step, "Step one frame", EditorGlyph::Step, ImGuiKey_F7, "F7"}};
    return commands;
}

const char* EditorCommandLabel(EditorCommand command, const EditorSnapshot& snapshot) {
    if (command == EditorCommand::Pause) return snapshot.paused ? "Resume" : "Pause";
    if (command == EditorCommand::Stop && snapshot.profiler.busy) return "Stop preview";
    if (command == EditorCommand::Refresh && snapshot.building) return "Building...";
    if (command == EditorCommand::Cancel) {
        if (snapshot.profiler.busy)
            return snapshot.benchmarkCancelPending ? "Canceling benchmark..." : "Cancel benchmark";
        if (snapshot.building) return "Cancel build";
        if (snapshot.phase == EditorPhase::ReadingCatalog) return "Cancel loading";
        if (snapshot.phase == EditorPhase::Starting) return "Cancel starting";
    }
    return CommandInfo(command).label;
}

EditorCommand EditorPrimaryCommand(const EditorSnapshot& snapshot) {
    return ResolveEditorPrimaryCommand(snapshot.phase, snapshot.capabilities, snapshot.profiler.busy);
}

void EditorCommandMenuItem(EditorCommand command, const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const bool active = command == EditorCommand::Pause && snapshot.paused;
    if (ImGui::MenuItem(EditorCommandLabel(command, snapshot), CommandShortcut(command, snapshot), active,
                        snapshot.capabilities.Can(command)) &&
        actions.command)
        actions.command(command);
    EditorTooltip(CommandTooltip(command, snapshot).c_str());
}

bool EditorCommandButton(EditorCommand command, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         bool showLabel) {
    ImGui::PushID(static_cast<int>(command));
    ImGui::BeginDisabled(!snapshot.capabilities.Can(command));
    ImGui::PushStyleColor(ImGuiCol_Text, CommandColor(command, snapshot));
    const bool active = command == EditorCommand::Pause && snapshot.paused;
    const auto tooltip = CommandTooltip(command, snapshot);
    bool pressed = false;
    if (showLabel) {
        const std::string label = std::string(EditorGlyphText(CommandGlyph(command, snapshot))) + "  " +
                                  EditorCommandLabel(command, snapshot);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
        pressed = ImGui::Button(label.c_str());
        if (active) ImGui::PopStyleColor();
        EditorTooltip(tooltip.c_str());
    } else {
        pressed = EditorIconButton("command", CommandGlyph(command, snapshot), tooltip.c_str(), active);
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    ImGui::PopID();
    if (pressed && actions.command) actions.command(command);
    return pressed;
}
