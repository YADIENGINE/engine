#include "Editor/Ui/Panels/Activity/ActivityPanel.h"
#include <imgui.h>
#include <array>
#include <ctime>
#include <string>
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float TimeColumnWidth = 72.0f;

std::string TimeLabel(std::chrono::system_clock::time_point time) {
    const auto value = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    localtime_s(&local, &value);
    std::array<char, 16> stamp{};
    std::strftime(stamp.data(), stamp.size(), "%H:%M:%S", &local);
    return stamp.data();
}
// Undo only while it would do something: the same scene is open and the value is still the agent's.
bool CanUndo(const EditorControlEvent& event, const EditorSnapshot& snapshot) {
    if (!event.previous || !snapshot.selected || snapshot.selected->id != event.scene) return false;
    const auto current = snapshot.parameters.find(event.parameter);
    return current != snapshot.parameters.end() && current->second != *event.previous;
}
} // namespace

void ActivityPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    if (!snapshot.controlEvents || snapshot.controlEvents->empty()) {
        ToolEmptyState("No agent activity yet", "What agent tools change or capture in this session appears here.");
        return;
    }
    const float scale = ToolUiScale();
    const auto& events = *snapshot.controlEvents;
    for (std::size_t index = events.size(); index-- > 0;) {
        const auto& event = events[index];
        ImGui::PushID(static_cast<int>(index));
        const float startX = ImGui::GetCursorPosX();
        ImGui::AlignTextToFramePadding();
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
        ImGui::TextColored(ToolPalette::Muted, "%s", TimeLabel(event.time).c_str());
        ImGui::PopFont();
        ImGui::SameLine(startX + TimeColumnWidth * scale);
        const bool undo = CanUndo(event, snapshot);
        const bool open = !event.file.empty();
        const char* label = undo ? "Undo" : open ? "Open" : nullptr;
        const float buttonWidth = label ? ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2 +
                                              ImGui::GetStyle().ItemSpacing.x
                                        : 0.0f;
        const auto text =
            ToolClipText(event.text.c_str(), std::max(1.0f, ImGui::GetContentRegionAvail().x - buttonWidth));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(text.c_str());
        if (text != event.text) ToolTooltip(event.text.c_str());
        if (label) {
            ImGui::SameLine(ImGui::GetContentRegionMax().x - buttonWidth + ImGui::GetStyle().ItemSpacing.x);
            if (ToolButton(label)) {
                if (undo)
                    actions.parameter(event.parameter, *event.previous);
                else
                    actions.openPath(event.file);
            }
        }
        ImGui::PopID();
    }
}
