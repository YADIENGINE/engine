#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <format>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float SearchMaximumWidth = 560.0f;
constexpr float SearchMinimumWidth = 120.0f;
constexpr float SearchHintMinimum = 100.0f;  // The shortcut shows only when the hint keeps this much room
constexpr float SearchComfortWidth = 280.0f; // The agent strip gives up its text before the box goes below this
std::string PlaybackLabel(double speed) {
    std::array<char, 16> label{};
    std::snprintf(label.data(), label.size(), "%.2gx", speed);
    return label.data();
}
float PlaybackSpeedWidth() {
    return ImGui::CalcTextSize("0.25x").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFontSize();
}
void PlaybackSpeed(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool inMenu) {
    ImGui::BeginDisabled(!snapshot.capabilities.playbackEnabled);
    constexpr std::array Speeds{0.1, 0.25, 0.5, 1.0, 2.0, 4.0};
    if (inMenu) {
        if (ImGui::BeginMenu("Playback speed")) {
            for (const double speed : Speeds)
                if (ImGui::MenuItem(PlaybackLabel(speed).c_str(), nullptr, speed == snapshot.playbackSpeed) &&
                    actions.playback)
                    actions.playback(speed, false);
            ImGui::EndMenu();
        }
    } else {
        ImGui::SetNextItemWidth(PlaybackSpeedWidth());
        if (ToolBeginCombo("##speed", PlaybackLabel(snapshot.playbackSpeed).c_str())) {
            for (const double speed : Speeds)
                if (ImGui::Selectable(PlaybackLabel(speed).c_str(), speed == snapshot.playbackSpeed) &&
                    actions.playback)
                    actions.playback(speed, false);
            ImGui::EndCombo();
        }
    }
    ImGui::EndDisabled();
    ToolTooltip(snapshot.capabilities.playbackEnabled ? "Playback speed"
                                                      : snapshot.capabilities.playbackDisabledReason);
}
// Looks like a search field and opens quick open, which appears right over it and takes the typing.
void SearchBox(float width, EditorToolbarSearch& search) {
    const float scale = ToolUiScale();
    const float height = ImGui::GetFrameHeight();
    const auto start = ImGui::GetCursorScreenPos();
    search.pressed = ImGui::InvisibleButton("quick-open", {width, height});
    search.centerX = start.x + width * 0.5f;
    search.top = start.y;
    const bool hovered = ImGui::IsItemHovered();
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, {start.x + width, start.y + height},
                        ImGui::GetColorU32(hovered ? ToolPalette::Elevated : ToolPalette::Panel),
                        ToolMetrics::Radius * scale);
    const float inset = EditorToolbarMetrics::ButtonPadding * scale;
    const float textY = start.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    const auto muted = ImGui::GetColorU32(ToolPalette::Muted);
    constexpr const char* Icon = ICON_FA_MAGNIFYING_GLASS;
    draw->AddText({start.x + inset, textY}, muted, Icon);
    const float textX = start.x + inset + ImGui::CalcTextSize(Icon).x + inset * 0.75f;
    float right = start.x + width - inset;
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    constexpr const char* Shortcut = "Ctrl+P";
    const float shortcutWidth = ImGui::CalcTextSize(Shortcut).x;
    if (right - shortcutWidth - textX > SearchHintMinimum * scale) {
        draw->AddText({right - shortcutWidth, start.y + (height - ImGui::GetTextLineHeight()) * 0.5f}, muted, Shortcut);
        right -= shortcutWidth + inset;
    }
    ImGui::PopFont();
    constexpr const char* Hint = "Search scenes, graphics, sounds & commands";
    const char* hint = ImGui::CalcTextSize(Hint).x <= right - textX ? Hint : "Quick open";
    draw->AddText({textX, textY}, muted, ToolClipText(hint, std::max(1.0f, right - textX)).c_str());
    ToolTooltip("Scenes, their choices, graphics, sounds and commands (Ctrl+P)");
}
} // namespace

void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool& marking,
                                EditorToolbarSearch& search, const EditorAgentStatus& agent,
                                EditorAgentActivity& activity) {
    const float scale = ToolUiScale();
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        {EditorToolbarMetrics::ButtonPadding * scale,
                         (EditorToolbarMetrics::ControlHeight - ToolMetrics::BodySize) * 0.5f * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        {EditorToolbarMetrics::Gap * scale, ImGui::GetStyle().ItemSpacing.y});
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::PushID("editor-playback");
    ImGui::BeginGroup();
    const float available = ImGui::GetContentRegionAvail().x;
    const float iconWidth = ImGui::GetFrameHeight();
    const float gap = EditorToolbarMetrics::Gap * scale;
    const float transportGap = EditorToolbarMetrics::TransportGap * scale;
    const float groupGap = EditorToolbarMetrics::GroupGap * scale;
    const auto primary = EditorPrimaryCommand(snapshot);
    const float primaryWidth = EditorCommandButtonWidth(primary, snapshot, true);
    const float buildLabelWidth = EditorCommandButtonWidth(EditorCommand::Refresh, snapshot, true);
    const float playLabelWidth = EditorCommandButtonWidth(EditorCommand::BuildAndPlay, snapshot, true);
    bool showSpeed = true;
    bool showReset = true;
    bool showStep = true;
    bool showBuildLabel = true;
    bool showBuild = true;
    bool showPrimaryLabel = true;
    const auto transportWidth = [&] {
        float width = (showPrimaryLabel ? primaryWidth : iconWidth) + iconWidth + transportGap;
        if (showStep) width += iconWidth + transportGap;
        if (showReset) width += iconWidth + transportGap;
        return width;
    };
    const auto overflow = [&] { return !showSpeed || !showReset || !showStep || !showBuild; };
    float agentWidth = 0;
    const auto toolsWidth = [&] {
        float width = iconWidth + agentWidth + gap; // Mark and the agent strip
        if (showSpeed) width += PlaybackSpeedWidth() + gap;
        if (showBuild) width += (showBuildLabel ? buildLabelWidth + playLabelWidth : iconWidth * 2) + gap * 2;
        if (overflow()) width += iconWidth + gap;
        return width;
    };
    const float searchMinimum = SearchMinimumWidth * scale;
    agentWidth = std::clamp(available - transportWidth() - toolsWidth() - SearchComfortWidth * scale - groupGap * 2,
                            EditorAgentActivityMetrics::Medium * scale, EditorAgentActivityMetrics::Ideal * scale);
    const auto fits = [&] { return transportWidth() + toolsWidth() + searchMinimum + groupGap * 2 <= available; };
    if (!fits()) showSpeed = false;
    if (!fits()) showReset = false;
    if (!fits()) showStep = false;
    if (!fits()) showBuildLabel = false;
    if (!fits()) showBuild = false;
    if (!fits()) showPrimaryLabel = false;
    if (!fits()) agentWidth = EditorAgentActivityMetrics::Compact * scale;
    // The search box sits in the window's center while playback and the tools leave it room, sliding when they do not.
    const float transport = transportWidth();
    const float tools = toolsWidth();
    const float room = std::max(1.0f, available - transport - tools - groupGap * 2);
    const float searchWidth = std::min(SearchMaximumWidth * scale, room);
    const float searchStart = std::clamp((available - searchWidth) * 0.5f, transport + groupGap,
                                         std::max(transport + groupGap, available - tools - groupGap - searchWidth));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {transportGap, ImGui::GetStyle().ItemSpacing.y});
    EditorCommandButton(primary, snapshot, actions, showPrimaryLabel);
    ImGui::SameLine();
    EditorCommandButton(EditorCommand::Pause, snapshot, actions);
    if (showStep) {
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::Step, snapshot, actions);
    }
    if (showReset) {
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::Reset, snapshot, actions);
    }
    ImGui::PopStyleVar();
    if (snapshot.simulation) { // Only in the room between playback and search; ctl status always has it
        const auto& simulation = *snapshot.simulation;
        const auto label = std::format("Step {}{}", simulation.step,
                                       simulation.differs     ? "  differs from the replay"
                                       : simulation.replaying ? "  replay"
                                                              : "");
        if (transport + groupGap * 2 + ImGui::CalcTextSize(label.c_str()).x <= searchStart) {
            ImGui::SameLine(transport + groupGap);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label.c_str());
        }
    }
    // Inside the group SameLine's offset counts from the group's start.
    ImGui::SameLine(searchStart);
    SearchBox(searchWidth, search);
    ImGui::SameLine(std::max(available - tools, searchStart + searchWidth + groupGap));
    if (showSpeed) {
        PlaybackSpeed(snapshot, actions, false);
        ImGui::SameLine();
    }
    if (overflow()) {
        if (EditorIconButton("more", EditorGlyph::More, "More playback controls")) ImGui::OpenPopup("more-controls");
        if (ImGui::BeginPopup("more-controls")) {
            if (!showStep) EditorCommandMenuItem(EditorCommand::Step, snapshot, actions);
            if (!showReset) EditorCommandMenuItem(EditorCommand::Reset, snapshot, actions);
            if (!showBuild) {
                EditorCommandMenuItem(EditorCommand::Refresh, snapshot, actions);
                EditorCommandMenuItem(EditorCommand::BuildAndPlay, snapshot, actions);
            }
            if (!showSpeed) {
                if (!showStep || !showReset || !showBuild) ImGui::Separator();
                PlaybackSpeed(snapshot, actions, true);
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();
    }
    if (EditorIconButton(
            "mark", EditorGlyph::Mark,
            "Mark a region of the scene or the editor and paste it into the Console (or Alt+drag on a scene)", marking))
        marking = !marking;
    if (showBuild) {
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::Refresh, snapshot, actions, showBuildLabel);
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::BuildAndPlay, snapshot, actions, showBuildLabel);
    }
    ImGui::SameLine();
    activity.Draw(snapshot, actions, agent, agentWidth);
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
}
