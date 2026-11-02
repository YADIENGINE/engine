#include "Editor/Ui/Commands/EditorCommands.h"
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Application/EditorCommandState.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float PrimaryWidthInFonts = 5.5f;
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
        EditorCommandInfo{EditorCommand::Open, "Run", EditorGlyph::Play, 0, nullptr},
        EditorCommandInfo{EditorCommand::Stop, "Stop", EditorGlyph::Stop, 0, nullptr},
        EditorCommandInfo{EditorCommand::Pause, "Pause", EditorGlyph::Pause, ImGuiKey_F6, "F6"},
        EditorCommandInfo{EditorCommand::Reset, "Reset", EditorGlyph::Reset, ImGuiMod_Ctrl | ImGuiKey_R, "Ctrl+R"},
        EditorCommandInfo{EditorCommand::Refresh, "Build", EditorGlyph::Build,
                          ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B, "Ctrl+Shift+B"},
        EditorCommandInfo{EditorCommand::Cancel, "Cancel operation", EditorGlyph::Cancel, 0, nullptr},
        EditorCommandInfo{EditorCommand::Step, "Step one frame", EditorGlyph::Step, ImGuiKey_F7, "F7"},
        EditorCommandInfo{EditorCommand::BuildAndPlay, "Play game", EditorGlyph::Play, 0, nullptr}};
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
    // Color says what a button does: green starts the scene, a rose tint stops it, a paused scene lights Resume in
    // amber. Build and Play game color only their icons; the rest stays neutral.
    const bool enabled = snapshot.capabilities.Can(command);
    const bool primary = command == EditorPrimaryCommand(snapshot);
    const bool starts = enabled && primary && command == EditorCommand::Open;
    const bool stops = enabled && primary && command == EditorCommand::Stop;
    const bool active = command == EditorCommand::Pause && snapshot.paused;
    ImGui::PushID(static_cast<int>(command));
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::BeginDisabled(!enabled);
    if (starts) PushEditorGoButton();
    if (stops) PushEditorTintedButton(ToolToneColor(ToolTone::Error), EditorStopTextColor());
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ToolPalette::Selected);
        ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::Accent);
    }
    std::string label =
        showLabel ? CommandButtonLabel(command, snapshot) : EditorGlyphText(CommandGlyph(command, snapshot));
    if (command == EditorCommand::BuildAndPlay || command == EditorCommand::Refresh) label.clear();
    label += "###command";
    const bool pressed =
        ToolButton(label.c_str(), {EditorCommandButtonWidth(command, snapshot, showLabel), ImGui::GetFrameHeight()});
    if (active) ImGui::PopStyleColor(2);
    if (starts || stops) PopEditorButton();
    if (command == EditorCommand::Refresh) {
        const auto start = ImGui::GetItemRectMin();
        const auto end = ImGui::GetItemRectMax();
        const char* glyph = EditorGlyphText(CommandGlyph(command, snapshot));
        const float y = start.y + ImGui::GetStyle().FramePadding.y;
        const float x = showLabel ? start.x + ImGui::GetStyle().FramePadding.x
                                  : (start.x + end.x - ImGui::CalcTextSize(glyph).x) * 0.5f;
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddText({x, y}, ImGui::GetColorU32(EditorInfoColor()), glyph);
        if (showLabel)
            draw->AddText({x + ImGui::CalcTextSize((std::string(glyph) + "  ").c_str()).x, y},
                          ImGui::GetColorU32(ImGuiCol_Text), EditorCommandLabel(command, snapshot));
    }
    if (command == EditorCommand::BuildAndPlay) {
        const auto start = ImGui::GetItemRectMin();
        const auto end = ImGui::GetItemRectMax();
        const float size = ImGui::GetFontSize();
        const float x = showLabel ? start.x + ImGui::GetStyle().FramePadding.x : (start.x + end.x - size) * 0.5f;
        DrawInvader({x, (start.y + end.y - size * 8.0f / 11.0f) * 0.5f}, size, ImGui::GetColorU32(ToolPalette::Accent));
        if (showLabel)
            ImGui::GetWindowDrawList()->AddText(
                {x + size + ImGui::CalcTextSize("  ").x, ImGui::GetItemRectMin().y + ImGui::GetStyle().FramePadding.y},
                ImGui::GetColorU32(ImGuiCol_Text), EditorCommandLabel(command, snapshot));
    }
    ImGui::EndDisabled();
    ImGui::PopFont();
    ToolTooltip(CommandTooltip(command, snapshot).c_str());
    ImGui::PopID();
    if (pressed && actions.command) actions.command(command);
    return pressed;
}
