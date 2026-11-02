#include "Editor/Ui/Widgets/EditorAgentActivity.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <utility>
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr const char* PopupId = "agent-activity";
constexpr float WorkingTint = 0.16f;
constexpr float DoneTint = 0.14f;
constexpr float HoverTint = 0.06f;
constexpr float FlashTint = 0.28f;
constexpr double FlashSeconds = 0.7;
constexpr double DoneHoldSeconds = 6.0; // Green stays this long, then fades to neutral
constexpr double DoneFadeSeconds = 2.0;
constexpr float Padding = 9.0f;
constexpr float Gap = 8.0f;
constexpr float CountPadding = 5.0f;
constexpr float PopupWidth = 500.0f;
constexpr float PopupMaximumListHeight = 360.0f;
constexpr float PopupOffset = 2.0f;
constexpr float TimeColumnWidth = 72.0f;
constexpr float IconColumnWidth = 22.0f;
constexpr float UnseenTint = 0.09f;

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
ImVec4 StateTone(SessionState state) {
    switch (state) {
    case SessionState::Waiting: return ToolToneColor(ToolTone::Warning);
    case SessionState::Done   : return ToolToneColor(ToolTone::Success);
    case SessionState::Working:
    case SessionState::Idle   :
    case SessionState::Exited : return EditorAgentColor();
    }
    return EditorAgentColor();
}
void DrawEvent(const EditorControlEvent& event, const EditorSnapshot& snapshot, const EditorUiActions& actions,
               bool unseen) {
    const float scale = ToolUiScale();
    const auto start = ImGui::GetCursorScreenPos();
    if (unseen) {
        auto tint = EditorAgentColor();
        tint.w = UnseenTint;
        ImGui::GetWindowDrawList()->AddRectFilled(
            start, {start.x + ImGui::GetContentRegionAvail().x, start.y + ImGui::GetFrameHeight()},
            ImGui::GetColorU32(tint));
    }
    const float startX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    ImGui::TextColored(ToolPalette::Muted, "%s", TimeLabel(event.time).c_str());
    ImGui::PopFont();
    ImGui::SameLine(startX + TimeColumnWidth * scale);
    const bool change = event.previous.has_value();
    const bool shot = !event.file.empty();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(change || shot ? EditorAgentColor() : ToolPalette::Muted, "%s",
                       change ? ICON_FA_SLIDERS
                       : shot ? ICON_FA_CAMERA
                              : ICON_FA_COMMENT);
    ImGui::SameLine(startX + (TimeColumnWidth + IconColumnWidth) * scale);
    const bool undo = CanUndo(event, snapshot);
    const char* label = undo ? "Undo" : shot ? "Open" : nullptr;
    const float buttonWidth =
        label ? ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2 + ImGui::GetStyle().ItemSpacing.x
              : 0.0f;
    const auto text = ToolClipText(event.text.c_str(), std::max(1.0f, ImGui::GetContentRegionAvail().x - buttonWidth));
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
}
} // namespace

bool EditorAgentActivity::TakeOpenSession() { return std::exchange(_openSession, false); }

void EditorAgentActivity::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                               const EditorAgentStatus& agent, float width) {
    const float scale = ToolUiScale();
    const double now = ImGui::GetTime();
    const auto state = agent.session ? agent.state : SessionState::Idle;
    if (state == SessionState::Done && _state != SessionState::Done) _doneAt = now;
    _state = state;
    const std::size_t total = snapshot.controlEventCount;
    if (total > _known) _flashAt = now;
    _known = total;

    const EditorControlEvent* last =
        snapshot.controlEvents && !snapshot.controlEvents->empty() ? &snapshot.controlEvents->back() : nullptr;
    const std::string lastText = last ? last->text : std::string{};
    const auto line = AgentHeadline(state, agent.activity, lastText);
    const std::string age =
        last && state != SessionState::Waiting && line == lastText
            ? AgentAgeLabel(std::chrono::duration<double>(std::chrono::system_clock::now() - last->time).count())
            : std::string{};
    const auto earlier = AgentEarlierLabel(total);
    const bool quiet = state == SessionState::Idle || state == SessionState::Exited;
    const auto tone = StateTone(state);

    float tint = 0;
    if (state == SessionState::Working) tint = WorkingTint;
    if (state == SessionState::Waiting) tint = EditorAttentionPulse();
    if (state == SessionState::Done)
        tint = DoneTint *
               (1.0f - static_cast<float>(std::clamp((now - _doneAt - DoneHoldSeconds) / DoneFadeSeconds, 0.0, 1.0)));
    if (state != SessionState::Waiting)
        tint += FlashTint * (1.0f - static_cast<float>(std::min((now - _flashAt) / FlashSeconds, 1.0)));

    ImGui::PushID("editor-agent-activity");
    const float height = ImGui::GetFrameHeight();
    const auto start = ImGui::GetCursorScreenPos();
    const ImVec2 end{start.x + width, start.y + height};
    const bool pressed = ImGui::InvisibleButton("strip", {width, height});
    const bool hovered = ImGui::IsItemHovered();
    if (pressed || std::exchange(_showRequested, false)) {
        ImGui::OpenPopup(PopupId);
        _seenAtOpen = _seen;
    }

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(
        start, end, ImGui::GetColorU32(EditorTintSurface(ToolPalette::Panel, tone, tint + (hovered ? HoverTint : 0))),
        ToolMetrics::Radius * scale);
    const float padding = Padding * scale;
    const float gap = Gap * scale;
    const float side = ToolActivitySide();
    ToolDrawActivity(*draw, {start.x + padding, start.y + (height - side) * 0.5f}, quiet ? ToolPalette::Muted : tone,
                     state == SessionState::Working);
    const float lineStart = start.x + padding + side + gap;
    float right = end.x - padding;
    const bool compact = width < EditorAgentActivityMetrics::Medium * scale;
    if (!compact) {
        const char* chevron = ICON_FA_CHEVRON_DOWN;
        ImGui::PushFont(ToolFont(ToolFontRole::Ui), ToolMetrics::CaptionSize);
        const auto size = ImGui::CalcTextSize(chevron);
        draw->AddText({right - size.x, start.y + (height - size.y) * 0.5f}, ImGui::GetColorU32(ToolPalette::Muted),
                      chevron);
        ImGui::PopFont();
        right -= size.x + gap;
    }
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    if (!earlier.empty()) {
        const bool unseen = total > _seen;
        const float countPadding = CountPadding * scale;
        const auto size = ImGui::CalcTextSize(earlier.c_str());
        const float top = start.y + (height - size.y) * 0.5f;
        const ImVec2 from{right - size.x - countPadding * 2, top - 1 * scale};
        draw->AddRectFilled(from, {right, top + size.y + 1 * scale},
                            ImGui::GetColorU32(unseen ? (quiet ? EditorAgentColor() : tone) : ToolPalette::Window),
                            ToolMetrics::Radius * scale);
        draw->AddText({from.x + countPadding, top},
                      ImGui::GetColorU32(unseen ? ToolPalette::Window : ToolPalette::Muted), earlier.c_str());
        right = from.x - gap;
    }
    if (!compact && !age.empty()) {
        const auto size = ImGui::CalcTextSize(age.c_str());
        draw->AddText({right - size.x, start.y + (height - size.y) * 0.5f}, ImGui::GetColorU32(ToolPalette::Muted),
                      age.c_str());
        right -= size.x + gap;
    }
    ImGui::PopFont();
    if (!compact && right > lineStart) {
        const auto clipped = ToolClipText(line.c_str(), right - lineStart);
        draw->AddText({lineStart, start.y + (height - ImGui::GetTextLineHeight()) * 0.5f},
                      ImGui::GetColorU32(quiet && !last ? ToolPalette::Muted : ToolPalette::Text), clipped.c_str());
    }
    if (!ImGui::IsPopupOpen(PopupId)) ToolTooltip((line + "\n\nClick to see what the agent did").c_str());

    const float popupWidth = PopupWidth * scale;
    ImGui::SetNextWindowPos({end.x, end.y + PopupOffset * scale}, ImGuiCond_Always, {1.0f, 0.0f});
    ImGui::SetNextWindowSize({popupWidth, 0});
    if (ImGui::BeginPopup(PopupId)) {
        _seen = total;
        DrawList(snapshot, actions, agent);
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

void EditorAgentActivity::DrawList(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                   const EditorAgentStatus& agent) {
    const float scale = ToolUiScale();
    const std::size_t total = snapshot.controlEventCount;
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Agent activity");
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(
        ToolPalette::Muted, "%s",
        (total == 0 ? std::string("nothing this session") : std::format("{} this session", total)).c_str());
    if (agent.session) {
        constexpr const char* Label = "Open session";
        const float buttonWidth = ImGui::CalcTextSize(Label).x + ImGui::GetStyle().FramePadding.x * 2;
        ImGui::SameLine(ImGui::GetContentRegionMax().x - buttonWidth);
        if (ToolButton(Label)) {
            _openSession = true;
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::Separator();
    if (!snapshot.controlEvents || snapshot.controlEvents->empty()) {
        ImGui::TextColored(ToolPalette::Muted, "What agent tools change or capture in this session appears here.");
        return;
    }
    const auto& events = *snapshot.controlEvents;
    const float rowHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y;
    const float listHeight = std::min(rowHeight * static_cast<float>(events.size()), PopupMaximumListHeight * scale);
    if (ImGui::BeginChild("rows", {0, listHeight})) {
        // A row's place in the whole session: the deque holds its latest entries.
        const std::size_t first = total - events.size();
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(events.size()), rowHeight);
        while (clipper.Step())
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const std::size_t index = events.size() - 1 - static_cast<std::size_t>(row);
                ImGui::PushID(static_cast<int>(first + index));
                DrawEvent(events[index], snapshot, actions, first + index >= _seenAtOpen);
                ImGui::PopID();
            }
    }
    ImGui::EndChild();
}
