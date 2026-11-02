#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float MinimumCrumbWidthInFonts = 6.0f;
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
// Where the open scene sits in the catalog: "Category / Scene", or only the scene when that does not fit.
void SceneCrumb(const EditorSnapshot& snapshot, float width) {
    if (!snapshot.selected) return;
    const auto& scene = *snapshot.selected;
    ImGui::AlignTextToFramePadding();
    const auto category = scene.category.empty() ? std::string() : scene.category + "  /  ";
    const float categoryWidth = ImGui::CalcTextSize(category.c_str()).x;
    const float nameWidth = ImGui::CalcTextSize(scene.name.c_str()).x;
    if (!category.empty() && categoryWidth + nameWidth <= width) {
        ImGui::TextDisabled("%s", category.c_str());
        ImGui::SameLine(0, 0);
        ImGui::TextUnformatted(scene.name.c_str());
    } else {
        ImGui::TextUnformatted(ToolClipText(scene.name.c_str(), width).c_str());
    }
}
} // namespace

void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool& marking) {
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
    const float origin = ImGui::GetCursorPosX();
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
    const auto toolsWidth = [&] {
        float width = iconWidth; // Mark
        if (showSpeed) width += PlaybackSpeedWidth() + gap;
        if (showBuild) width += (showBuildLabel ? buildLabelWidth + playLabelWidth : iconWidth * 2) + gap * 2;
        if (overflow()) width += iconWidth + gap;
        return width;
    };
    const auto fits = [&] { return transportWidth() + toolsWidth() + groupGap <= available; };
    if (!fits()) showSpeed = false;
    if (!fits()) showReset = false;
    if (!fits()) showStep = false;
    if (!fits()) showBuildLabel = false;
    if (!fits()) showBuild = false;
    if (!fits()) showPrimaryLabel = false;
    // Playback sits in the window's center while the tools leave it room, sliding left when they do not.
    const float transport = transportWidth();
    const float tools = toolsWidth();
    const float transportStart =
        std::clamp((available - transport) * 0.5f, 0.0f, std::max(0.0f, available - tools - groupGap - transport));
    const float crumbWidth = transportStart - groupGap;
    const bool showCrumb = snapshot.selected && crumbWidth >= ImGui::GetFontSize() * MinimumCrumbWidthInFonts;
    // Inside the group SameLine's offset counts from the group's start, SetCursorPosX's from the window's.
    if (showCrumb) {
        SceneCrumb(snapshot, crumbWidth);
        ImGui::SameLine(transportStart);
    } else if (transportStart > 0) {
        ImGui::SetCursorPosX(origin + transportStart);
    }
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
    ImGui::SameLine(std::max(available - tools, transportStart + transport + groupGap));
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
    if (!snapshot.ready) marking = false;
    ImGui::BeginDisabled(!snapshot.ready);
    if (EditorIconButton("mark", EditorGlyph::Mark,
                         "Mark a region of the scene and paste it into the Console (or Alt+drag)", marking))
        marking = !marking;
    ImGui::EndDisabled();
    if (showBuild) {
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::Refresh, snapshot, actions, showBuildLabel);
        ImGui::SameLine();
        EditorCommandButton(EditorCommand::BuildAndPlay, snapshot, actions, showBuildLabel);
    }
    ImGui::EndGroup();
    ImGui::PopID();
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar(2);
    ImGui::PopFont();
}
