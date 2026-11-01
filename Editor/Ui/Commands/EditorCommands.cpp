#include "Editor/Ui/Commands/EditorCommands.h"
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Application/EditorCommandState.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float PrimaryWidthInFonts = 8.0f;
void DrawInvader(ImVec2 position, float size, ImU32 color) {
    constexpr std::array<unsigned, 8> Rows{0x104, 0x088, 0x1fc, 0x376, 0x7ff, 0x5fd, 0x505, 0x0d8};
    const float pixel = size / 11.0f;
    auto* draw = ImGui::GetWindowDrawList();
    for (std::size_t y = 0; y < Rows.size(); ++y)
        for (unsigned x = 0; x < 11; ++x)
            if (Rows[y] & (1u << x)) {
                const ImVec2 start{position.x + static_cast<float>(x) * pixel,
                                   position.y + static_cast<float>(y) * pixel};
                draw->AddRectFilled(start, {start.x + pixel, start.y + pixel}, color);
            }
}
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
std::string CommandButtonLabel(EditorCommand command, const EditorSnapshot& snapshot) {
    return std::string(EditorGlyphText(CommandGlyph(command, snapshot))) + "  " + EditorCommandLabel(command, snapshot);
}
ImVec4 CommandColor(EditorCommand command, const EditorSnapshot& snapshot) {
    switch (command) {
    case EditorCommand::Open   : return ToolToneColor(ToolTone::Success);
    case EditorCommand::BuildAndPlay: return ToolToneColor(ToolTone::Success);
    case EditorCommand::Stop   : return ToolToneColor(ToolTone::Error);
    case EditorCommand::Pause  : return ToolToneColor(snapshot.paused ? ToolTone::Success : ToolTone::Warning);
    case EditorCommand::Refresh: return {192.0f / 255, 164.0f / 255, 235.0f / 255, 1};
    case EditorCommand::Cancel : return ToolToneColor(ToolTone::Warning);
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
        EditorCommandInfo{EditorCommand::Refresh, "Build", EditorGlyph::Build,
                          ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B, "Ctrl+Shift+B"},
        EditorCommandInfo{EditorCommand::Cancel, "Cancel operation", EditorGlyph::Cancel, 0, nullptr},
        EditorCommandInfo{EditorCommand::Step, "Step one frame", EditorGlyph::Step, ImGuiKey_F7, "F7"},
        EditorCommandInfo{EditorCommand::BuildAndPlay, "Build And Play", EditorGlyph::Play, 0, nullptr}};
    return commands;
}

const char* EditorCommandLabel(EditorCommand command, const EditorSnapshot& snapshot) {
    if (command == EditorCommand::Pause) return snapshot.paused ? "Resume" : "Pause";
    if (command == EditorCommand::Stop && snapshot.profiler.busy) return "Stop scene";
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
    ToolTooltip(CommandTooltip(command, snapshot).c_str());
}

float EditorCommandButtonWidth(EditorCommand command, const EditorSnapshot& snapshot, bool showLabel) {
    if (!showLabel) return ImGui::GetFrameHeight();
    if (command == EditorCommand::BuildAndPlay)
        return ImGui::GetFontSize() + ImGui::CalcTextSize("  ").x +
               ImGui::CalcTextSize(EditorCommandLabel(command, snapshot)).x + ImGui::GetStyle().FramePadding.x * 2;
    const float width =
        ImGui::CalcTextSize(CommandButtonLabel(command, snapshot).c_str()).x + ImGui::GetStyle().FramePadding.x * 2;
    return command == EditorPrimaryCommand(snapshot) ? std::max(width, ImGui::GetFontSize() * PrimaryWidthInFonts)
                                                     : width;
}

bool EditorCommandButton(EditorCommand command, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         bool showLabel) {
    const auto tone = CommandColor(command, snapshot);
    const auto base = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
    auto background = ImGui::GetStyleColorVec4(ImGuiCol_Button);
    auto hovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
    auto pressedColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
    auto border = ImGui::GetStyleColorVec4(ImGuiCol_Border);
    auto text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const bool active = command == EditorCommand::Pause && snapshot.paused;
    if (command == EditorPrimaryCommand(snapshot)) {
        background = tone;
        hovered = ImLerp(tone, ToolPalette::Text, 0.12f);
        pressedColor = ImLerp(tone, ToolPalette::Content, 0.16f);
        border = tone;
        text = ToolPalette::Content;
    } else if (command == EditorCommand::Pause || command == EditorCommand::Refresh ||
               command == EditorCommand::BuildAndPlay) {
        background = ImLerp(base, tone, active ? 0.32f : 0.22f);
        hovered = ImLerp(base, tone, active ? 0.42f : 0.34f);
        pressedColor = ImLerp(base, tone, 0.46f);
        border = ImLerp(border, tone, 0.48f);
        text = tone;
    }
    ImGui::PushID(static_cast<int>(command));
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::BeginDisabled(!snapshot.capabilities.Can(command));
    ImGui::PushStyleColor(ImGuiCol_Button, background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressedColor);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    ImGui::PushStyleColor(ImGuiCol_Text, text);
    std::string label =
        showLabel ? CommandButtonLabel(command, snapshot) : EditorGlyphText(CommandGlyph(command, snapshot));
    if (command == EditorCommand::BuildAndPlay) label.clear();
    label += "###command";
    const bool pressed =
        ToolButton(label.c_str(), {EditorCommandButtonWidth(command, snapshot, showLabel), ImGui::GetFrameHeight()});
    if (command == EditorCommand::BuildAndPlay) {
        const auto start = ImGui::GetItemRectMin();
        const auto end = ImGui::GetItemRectMax();
        const float size = ImGui::GetFontSize();
        const float x = showLabel ? start.x + ImGui::GetStyle().FramePadding.x : (start.x + end.x - size) * 0.5f;
        const auto color = ImGui::GetColorU32(ImGuiCol_Text);
        DrawInvader({x, (start.y + end.y - size * 8.0f / 11.0f) * 0.5f}, size, color);
        if (showLabel)
            ImGui::GetWindowDrawList()->AddText(
                {x + size + ImGui::CalcTextSize("  ").x, (start.y + end.y - size) * 0.5f}, color,
                EditorCommandLabel(command, snapshot));
    }
    ImGui::PopStyleColor(5);
    ImGui::EndDisabled();
    ImGui::PopFont();
    ToolTooltip(CommandTooltip(command, snapshot).c_str());
    ImGui::PopID();
    if (pressed && actions.command) actions.command(command);
    return pressed;
}
