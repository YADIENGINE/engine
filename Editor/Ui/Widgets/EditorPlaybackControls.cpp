#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include <imgui.h>
#include <array>
#include <cstdio>
#include <string>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
constexpr float ButtonTooltipDelay = 1.0f;
constexpr float IconButtonScale = 1.2f;
std::string PlaybackLabel(double speed) {
    std::array<char, 16> label{};
    std::snprintf(label.data(), label.size(), "%.2gx", speed);
    return label.data();
}
} // namespace

void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    struct Button {
        EditorCommand command;
        EditorGlyph glyph;
        const char* label;
    };
    const std::array buttons{Button{EditorCommand::Open, EditorGlyph::Play, snapshot.ready ? "Restart" : "Open"},
                             Button{EditorCommand::Pause, snapshot.paused ? EditorGlyph::Play : EditorGlyph::Pause,
                                    snapshot.paused ? "Resume" : "Pause"},
                             Button{EditorCommand::Stop, EditorGlyph::Stop, "Stop"},
                             Button{EditorCommand::Reset, EditorGlyph::Reset, "Reset"},
                             Button{EditorCommand::Refresh, EditorGlyph::Build, "Build & Refresh"},
                             Button{EditorCommand::Cancel, EditorGlyph::Cancel, "Cancel"}};
    auto& style = ImGui::GetStyle();
    const float previousHoverDelay = style.HoverDelayNormal;
    style.HoverDelayNormal = ButtonTooltipDelay;
    bool first = true;
    for (const auto& button : buttons) {
        if (button.command == EditorCommand::Cancel && !snapshot.busy) continue;
        if (!first) ImGui::SameLine();
        ImGui::PushID(static_cast<int>(button.command));
        ImGui::BeginDisabled(!snapshot.capabilities.Can(button.command));
        const bool pauseActive = button.command == EditorCommand::Pause && snapshot.paused;
        if (pauseActive) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_HeaderActive));
        const float size = ImGui::GetFrameHeight() * IconButtonScale;
        ImGui::PushFont(nullptr, style.FontSizeBase * IconButtonScale);
        if (ImGui::Button(EditorGlyphText(button.glyph), {size, size})) actions.command(button.command);
        ImGui::PopFont();
        if (pauseActive) ImGui::PopStyleColor();
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled | ImGuiHoveredFlags_DelayNormal |
                                 ImGuiHoveredFlags_NoSharedDelay))
            EditorTooltip(button.label);
        ImGui::PopID();
        first = false;
    }
    style.HoverDelayNormal = previousHoverDelay;
    ImGui::BeginDisabled(!snapshot.ready);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetFontSize() * 6.0f);
    if (ImGui::BeginCombo("##speed", PlaybackLabel(snapshot.playbackSpeed).c_str())) {
        constexpr std::array Speeds{0.1, 0.25, 0.5, 1.0, 2.0, 4.0};
        for (const double speed : Speeds)
            if (ImGui::Selectable(PlaybackLabel(speed).c_str(), speed == snapshot.playbackSpeed))
                actions.playback(speed, false);
        ImGui::EndCombo();
    }
    EditorTooltip("Playback speed");
    ImGui::SameLine();
    ImGui::BeginDisabled(!snapshot.paused);
    if (ImGui::Button("Step 1/60s")) actions.playback(snapshot.playbackSpeed, true);
    ImGui::EndDisabled();
    ImGui::EndDisabled();
}
