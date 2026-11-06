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
constexpr float TimeColumnWidth = 72.0f;
constexpr float IconColumnWidth = 22.0f;
constexpr float UnseenTint = 0.09f;
constexpr float DotRadius = 3.5f;
constexpr float DotGap = 4.0f;
constexpr std::size_t MaximumDots = 5; // More sessions show as "+N" after the last dot

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
} // namespace

void DrawEditorAgentEventRow(const EditorControlEvent& event, const EditorSnapshot& snapshot,
                             const EditorUiActions& actions, bool highlighted) {
    const float scale = ToolUiScale();
    const auto start = ImGui::GetCursorScreenPos();
    if (highlighted) {
        auto tint = EditorAgentColor();
        tint.w = UnseenTint;
        ImGui::GetWindowDrawList()->AddRectFilled(
            start, {start.x + ImGui::GetContentRegionAvail().x, start.y + ImGui::GetFrameHeight()},
            ImGui::GetColorU32(tint));
    }
    const float startX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    ImGui::TextColored(ToolColors().muted, "%s", TimeLabel(event.time).c_str());
    ImGui::PopFont();
    ImGui::SameLine(startX + TimeColumnWidth * scale);
    const bool change = event.previous.has_value();
    const bool shot = !event.file.empty();
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(change || shot ? EditorAgentColor() : ToolColors().muted, "%s",
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

bool EditorAgentActivity::TakeOpenAgents() { return std::exchange(_openAgents, false); }

void EditorAgentActivity::Draw(const EditorSnapshot& snapshot, const EditorUiActions&, const EditorAgentStatus& agent,
                               float width) {
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
        _openAgents = true;
        _seen = total;
    }

    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, end,
                        ImGui::GetColorU32(EditorTintSurface(ToolColors().panel, tone,
                                                             tint + (hovered || agent.drawerOpen ? HoverTint : 0) +
                                                                 (agent.drawerOpen ? HoverTint : 0))),
                        ToolMetrics::Radius * scale);
    const float padding = Padding * scale;
    const float gap = Gap * scale;
    const float side = ToolActivitySide();
    ToolDrawActivity(*draw, {start.x + padding, start.y + (height - side) * 0.5f}, quiet ? ToolColors().muted : tone,
                     state == SessionState::Working);
    float lineStart = start.x + padding + side + gap;
    float right = end.x - padding;
    const bool compact = width < EditorAgentActivityMetrics::Medium * scale;
    // Several agents: one dot each, so a waiting one shows even when the line speaks of another.
    if (agent.sessions.size() > 1 && !compact) {
        const float radius = DotRadius * scale;
        const float centerY = start.y + height * 0.5f;
        float x = lineStart + radius;
        // A narrower strip keeps fewer dots, so the line still has room
        const auto shown = std::min(agent.sessions.size(),
                                    width < EditorAgentActivityMetrics::Ideal * scale ? MaximumDots - 2 : MaximumDots);
        for (std::size_t index = 0; index < shown; ++index) {
            const auto& dot = agent.sessions[index];
            const auto color =
                dot.state == SessionState::Idle ? ToolColors().muted : EditorSessionStateColor(dot.state);
            draw->AddCircleFilled({x, centerY}, radius, ImGui::GetColorU32(color));
            if (dot.holds)
                draw->AddCircle({x, centerY}, radius + 2.0f * scale, ImGui::GetColorU32(EditorAgentColor()), 0,
                                1.0f * scale);
            x += radius * 2 + DotGap * scale;
        }
        if (agent.sessions.size() > shown) {
            ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
            const auto more = std::format("+{}", agent.sessions.size() - shown);
            const auto size = ImGui::CalcTextSize(more.c_str());
            draw->AddText({x - radius, centerY - size.y * 0.5f}, ImGui::GetColorU32(ToolColors().muted), more.c_str());
            x += size.x;
            ImGui::PopFont();
        }
        lineStart = x - radius + gap;
    }
    const bool heldByOther = snapshot.agents && snapshot.agents->hold &&
                             (!agent.session || snapshot.agents->hold->session != agent.sessionId);
    if (!compact) {
        const char* open = agent.drawerOpen ? ICON_FA_CHEVRON_UP : ICON_FA_CHEVRON_DOWN;
        const auto size = ImGui::CalcTextSize(open);
        draw->AddText({right - size.x, start.y + (height - size.y) * 0.5f}, ImGui::GetColorU32(ToolColors().muted),
                      open);
        right -= size.x + gap;
    }
    if (heldByOther && agent.sessions.size() < 2) { // With dots, the holder's dot wears a ring instead
        const auto size = ImGui::CalcTextSize(ICON_FA_LOCK);
        draw->AddText({right - size.x, start.y + (height - size.y) * 0.5f}, ImGui::GetColorU32(EditorAgentColor()),
                      ICON_FA_LOCK);
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
                            ImGui::GetColorU32(unseen ? (quiet ? EditorAgentColor() : tone) : ToolColors().window),
                            ToolMetrics::Radius * scale);
        draw->AddText({from.x + countPadding, top},
                      ImGui::GetColorU32(unseen ? ToolColors().window : ToolColors().muted), earlier.c_str());
        right = from.x - gap;
    }
    if (!compact && !age.empty()) {
        const auto size = ImGui::CalcTextSize(age.c_str());
        draw->AddText({right - size.x, start.y + (height - size.y) * 0.5f}, ImGui::GetColorU32(ToolColors().muted),
                      age.c_str());
        right -= size.x + gap;
    }
    ImGui::PopFont();
    if (!compact && right > lineStart) {
        // Several agents: the line names its session; a narrower strip keeps only the dots (its colour still says
        // that one waits)
        const bool several = agent.session && agent.sessions.size() > 1;
        const auto said = !several                                            ? line
                          : width < EditorAgentActivityMetrics::Ideal * scale ? std::string()
                                                                              : agent.title + ": " + line;
        const auto clipped = ToolClipText(said.c_str(), right - lineStart);
        draw->AddText({lineStart, start.y + (height - ImGui::GetTextLineHeight()) * 0.5f},
                      ImGui::GetColorU32(quiet && !last ? ToolColors().muted : ToolColors().text), clipped.c_str());
    }
    ToolTooltip(((agent.session ? agent.title + ": " : std::string()) + line +
                 (heldByOther ? "\nAnother session holds the editor." : "") + "\n\nClick for Agents")
                    .c_str());
    ImGui::PopID();
}
