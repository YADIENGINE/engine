#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include <imgui.h>
#include <array>
#include <cstdio>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
constexpr float SpeedWidthInFonts = 4.5f;
std::string PlaybackLabel(double speed) {
    std::array<char, 16> label{};
    std::snprintf(label.data(), label.size(), "%.2gx", speed);
    return label.data();
}
std::string CommandButtonLabel(EditorCommand command, const EditorSnapshot& snapshot) {
    const auto& info = EditorCommands()[static_cast<std::size_t>(command)];
    return std::string(EditorGlyphText(info.glyph)) + "  " + EditorCommandLabel(command, snapshot);
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
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * SpeedWidthInFonts);
        if (ImGui::BeginCombo("##speed", PlaybackLabel(snapshot.playbackSpeed).c_str())) {
            for (const double speed : Speeds)
                if (ImGui::Selectable(PlaybackLabel(speed).c_str(), speed == snapshot.playbackSpeed) &&
                    actions.playback)
                    actions.playback(speed, false);
            ImGui::EndCombo();
        }
    }
    ImGui::EndDisabled();
    EditorTooltip(snapshot.capabilities.playbackEnabled ? "Playback speed"
                                                        : snapshot.capabilities.playbackDisabledReason);
}
} // namespace

void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    ImGui::PushID("editor-playback");
    ImGui::BeginGroup();
    const auto& style = ImGui::GetStyle();
    const float available = ImGui::GetContentRegionAvail().x;
    const float iconWidth = ImGui::GetFrameHeight();
    const float gap = style.ItemSpacing.x;
    const auto primary = EditorPrimaryCommand(snapshot);
    const float primaryWidth =
        ImGui::CalcTextSize(CommandButtonLabel(primary, snapshot).c_str()).x + style.FramePadding.x * 2;
    const float buildLabelWidth =
        ImGui::CalcTextSize(CommandButtonLabel(EditorCommand::Refresh, snapshot).c_str()).x + style.FramePadding.x * 2;
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
            width += ImGui::GetFontSize() * SpeedWidthInFonts;
            ++controls;
        }
        if (overflow) {
            width += iconWidth;
            ++controls;
        }
        width += gap * static_cast<float>(controls - 1);
        if (showBuild) width += (showBuildLabel ? buildLabelWidth : iconWidth) + gap * 4 + 1;
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
    if (showSpeed) {
        ImGui::SameLine();
        PlaybackSpeed(snapshot, actions, false);
    }
    if (!showSpeed || !showReset || !showStep || !showBuild) {
        ImGui::SameLine();
        if (EditorIconButton("more", EditorGlyph::More, "More playback controls")) ImGui::OpenPopup("more-controls");
        if (ImGui::BeginPopup("more-controls")) {
            if (!showStep) EditorCommandMenuItem(EditorCommand::Step, snapshot, actions);
            if (!showReset) EditorCommandMenuItem(EditorCommand::Reset, snapshot, actions);
            if (!showSpeed) PlaybackSpeed(snapshot, actions, true);
            if (!showBuild) {
                ImGui::Separator();
                EditorCommandMenuItem(EditorCommand::Refresh, snapshot, actions);
            }
            ImGui::EndPopup();
        }
    }
    if (showBuild) {
        ImGui::SameLine(0, gap * 2);
        const auto separator = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddLine({separator.x, separator.y + style.FramePadding.y},
                                            {separator.x, separator.y + iconWidth - style.FramePadding.y},
                                            ImGui::GetColorU32(ImGuiCol_Separator));
        ImGui::Dummy({1, iconWidth});
        ImGui::SameLine(0, gap * 2);
        EditorCommandButton(EditorCommand::Refresh, snapshot, actions, showBuildLabel);
    }
    ImGui::EndGroup();
    ImGui::PopID();
}
