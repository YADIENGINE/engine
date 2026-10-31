#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include <imgui.h>
#include <array>
#include <cstdio>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
std::string PlaybackLabel(double speed) {
    std::array<char, 16> label{};
    std::snprintf(label.data(), label.size(), "%.2gx", speed);
    return label.data();
}
float PlaybackSpeedWidth() {
    return ImGui::CalcTextSize("0.25x").x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetFrameHeight();
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
        if (ImGui::BeginCombo("##speed", PlaybackLabel(snapshot.playbackSpeed).c_str())) {
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
} // namespace

void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const float scale = ToolUiScale();
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                        {EditorToolbarMetrics::ButtonPadding * scale,
                         (EditorToolbarMetrics::ControlHeight - ToolMetrics::BodySize) * 0.5f * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, EditorToolbarMetrics::Radius * scale);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        {EditorToolbarMetrics::Gap * scale, ImGui::GetStyle().ItemSpacing.y});
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_Button));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    ImGui::PushID("editor-playback");
    ImGui::BeginGroup();
    const auto& style = ImGui::GetStyle();
    const float available = ImGui::GetContentRegionAvail().x;
    const float iconWidth = ImGui::GetFrameHeight();
    const float gap = style.ItemSpacing.x;
    const auto primary = EditorPrimaryCommand(snapshot);
    const float primaryWidth = EditorCommandButtonWidth(primary, snapshot, true);
    const float buildLabelWidth = EditorCommandButtonWidth(EditorCommand::Refresh, snapshot, true);
    bool showSpeed = true;
    bool showReset = true;
    bool showStep = true;
    bool showBuildLabel = true;
    bool showBuild = true;
    bool showPrimaryLabel = true;
    const auto rowWidth = [&] {
        const bool overflow = !showSpeed || !showReset || !showStep || !showBuild;
        int controls = 2;
        float width = (showPrimaryLabel ? primaryWidth : iconWidth) + iconWidth;
        if (showStep) {
            width += iconWidth;
            ++controls;
        }
        if (showReset) {
            width += iconWidth;
            ++controls;
        }
        if (showSpeed) {
            width += PlaybackSpeedWidth();
            ++controls;
        }
        if (overflow) {
            width += iconWidth;
            ++controls;
        }
        width += gap * static_cast<float>(controls - 1);
        if (showBuild) width += (showBuildLabel ? buildLabelWidth : iconWidth) + gap;
        if (showBuild && showSpeed) width += gap * 3 + 1;
        return width;
    };
    if (rowWidth() > available) showSpeed = false;
    if (rowWidth() > available) showReset = false;
    if (rowWidth() > available) showStep = false;
    if (rowWidth() > available) showBuildLabel = false;
    if (rowWidth() > available) showBuild = false;
    if (rowWidth() > available) showPrimaryLabel = false;
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
    if (showBuild) {
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::Refresh, snapshot, actions, showBuildLabel);
    }
    if (showSpeed) {
        if (showBuild) {
            ImGui::SameLine(0, gap * 2);
            const auto separator = ImGui::GetCursorScreenPos();
            const float inset = EditorToolbarMetrics::Gap * scale;
            ImGui::GetWindowDrawList()->AddLine({separator.x, separator.y + inset},
                                                {separator.x, separator.y + iconWidth - inset},
                                                ImGui::GetColorU32(ImGuiCol_Separator));
            ImGui::Dummy({1, iconWidth});
            ImGui::SameLine(0, gap * 2);
        } else {
            ImGui::SameLine();
        }
        PlaybackSpeed(snapshot, actions, false);
    }
    if (!showSpeed || !showReset || !showStep || !showBuild) {
        ImGui::SameLine();
        if (EditorIconButton("more", EditorGlyph::More, "More playback controls")) ImGui::OpenPopup("more-controls");
        if (ImGui::BeginPopup("more-controls")) {
            if (!showStep) EditorCommandMenuItem(EditorCommand::Step, snapshot, actions);
            if (!showReset) EditorCommandMenuItem(EditorCommand::Reset, snapshot, actions);
            if (!showBuild) EditorCommandMenuItem(EditorCommand::Refresh, snapshot, actions);
            if (!showSpeed) {
                if (!showStep || !showReset || !showBuild) ImGui::Separator();
                PlaybackSpeed(snapshot, actions, true);
            }
            ImGui::EndPopup();
        }
    }
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(3);
    ImGui::PopFont();
}
