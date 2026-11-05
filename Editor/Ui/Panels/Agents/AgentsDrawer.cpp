#include "Editor/Ui/Panels/Agents/AgentsDrawer.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <ctime>
#include <format>
#include <functional>
#include <utility>
#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorAgentActivity.h"
#include "Terminal/TerminalSession.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr ImGuiWindowFlags DrawerFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr ImGuiWindowFlags StillChild = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
constexpr float DefaultWidth = 560.0f; // Logical pixels, until the developer drags the corner
constexpr float DefaultHeight = 440.0f;
constexpr float DefaultWidthShare = 0.30f; // Of the workspace, when that is wider than DefaultWidth suggests
constexpr float DefaultHeightShare = 0.5f;
constexpr float DefaultWidthLimit = 0.45f; // The default never covers more than this of the workspace
constexpr float DefaultHeightLimit = 0.65f;
constexpr float MinimumWidth = 380.0f;
constexpr float MinimumHeight = 280.0f;
constexpr float EdgeMargin = 6.0f; // From the workspace's top right corner
constexpr float Padding = 10.0f;
constexpr float GripSize = 16.0f; // The bottom-left corner that resizes it
constexpr float ChipDot = 3.5f;
constexpr float EmptyIconSize = 34.0f; // The robot of the empty state, in font pixels
constexpr float EmptyButtonWidth = 260.0f;
constexpr float ChipTitleMaximum = 150.0f;
constexpr float TimeColumnWidth = 72.0f;
constexpr float IconColumnWidth = 22.0f;
constexpr const char* OutsideTitle = "Outside the Console";

// One row of the list: a Console agent session, or every client outside the Console.
struct Row {
    std::string key; // Console session id as text; "" outside the Console
    std::optional<std::uint64_t> id;
    std::string title;
    SessionState state = SessionState::Idle;
    std::string activity;
    std::optional<std::chrono::system_clock::time_point> activityTime;
    std::optional<std::chrono::steady_clock::time_point> started;
    bool holds = false;
};

std::chrono::system_clock::time_point SystemTime(std::chrono::steady_clock::time_point time) {
    return std::chrono::system_clock::now() -
           std::chrono::duration_cast<std::chrono::system_clock::duration>(std::chrono::steady_clock::now() - time);
}
const AgentSessionReport* ReportOf(const EditorSnapshot& snapshot, const std::string& key) {
    if (!snapshot.agents) return nullptr;
    const auto found = snapshot.agents->sessions.find(key);
    return found == snapshot.agents->sessions.end() ? nullptr : &found->second;
}
// The latest thing a session said or did: its note, else its last tool.
void LatestActivity(Row& row, const AgentSessionReport* report) {
    if (!report) return;
    if (report->note && !report->note->text.empty()) {
        row.activity = report->note->text;
        row.activityTime = SystemTime(report->noteTime);
    }
    if (!report->tools.empty() && (!row.activityTime || report->tools.back().time > *row.activityTime)) {
        row.activity = report->tools.back().text;
        row.activityTime = report->tools.back().time;
    }
}
bool HasOutsideData(const EditorSnapshot& snapshot) {
    if (const auto* report = ReportOf(snapshot, ""); report && report->note) return true;
    if (snapshot.agents && snapshot.agents->hold && snapshot.agents->hold->session.empty()) return true;
    return snapshot.controlEvents && std::ranges::any_of(*snapshot.controlEvents, [](const EditorControlEvent& event) {
               return event.session.empty();
           });
}
// Agent sessions in the Console's order (a row never jumps while you aim at it), then the outside row when it has
// anything to show. Shells are not agents and stay in the Console.
std::vector<Row> RowsOf(const std::vector<ConsoleSessionInfo>& sessions, const EditorSnapshot& snapshot) {
    std::vector<Row> rows;
    const auto* hold = snapshot.agents && snapshot.agents->hold ? &*snapshot.agents->hold : nullptr;
    for (const auto& info : sessions) {
        if (!info.agent) continue;
        Row row{std::to_string(info.id), info.id, info.title, info.state, {}, {}, info.started, false};
        LatestActivity(row, ReportOf(snapshot, row.key));
        if (row.activity.empty() && !info.activity.empty()) {
            row.activity = info.activity;
            if (info.lastOutput) row.activityTime = SystemTime(*info.lastOutput);
        }
        row.holds = hold && hold->session == row.key;
        rows.push_back(std::move(row));
    }
    if (HasOutsideData(snapshot)) {
        Row row{"", std::nullopt, OutsideTitle, SessionState::Idle,
                {}, {},           std::nullopt, hold && hold->session.empty()};
        LatestActivity(row, ReportOf(snapshot, ""));
        // Nothing tells whether such an agent is still running; holding the editor says it worked lately.
        row.state = row.holds ? SessionState::Working : SessionState::Idle;
        if (snapshot.controlEvents && row.activity.empty())
            for (const auto& event : *snapshot.controlEvents)
                if (event.session.empty()) {
                    row.activity = event.text;
                    row.activityTime = event.time;
                }
        rows.push_back(std::move(row));
    }
    return rows;
}
// The row the drawer shows: the chosen one while it exists, else the first.
const Row* Chosen(const std::vector<Row>& rows, const std::optional<std::string>& selected) {
    if (rows.empty()) return nullptr;
    if (selected)
        if (const auto found = std::ranges::find(rows, *selected, &Row::key); found != rows.end()) return &*found;
    return &rows.front();
}
const char* StateLabel(SessionState state) {
    switch (state) {
    case SessionState::Idle   : return "idle";
    case SessionState::Working: return "working";
    case SessionState::Waiting: return "waiting for you";
    case SessionState::Done   : return "done";
    case SessionState::Exited : return "exited";
    }
    return "idle";
}
std::string Age(std::chrono::system_clock::time_point time) {
    const double seconds =
        std::max(0.0, std::chrono::duration<double>(std::chrono::system_clock::now() - time).count());
    if (seconds < 60) return "now";
    if (seconds < 3600) return std::format("{}m", static_cast<int>(seconds / 60));
    return std::format("{}h", static_cast<int>(seconds / 3600));
}
std::string Running(std::chrono::steady_clock::time_point started) {
    const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(std::chrono::steady_clock::now() - started);
    if (minutes.count() < 1) return "just started";
    if (minutes.count() < 60) return std::format("running {}m", minutes.count());
    return std::format("running {}h {}m", minutes.count() / 60, minutes.count() % 60);
}
std::string Clock(std::chrono::system_clock::time_point time) {
    const auto value = std::chrono::system_clock::to_time_t(time);
    std::tm local{};
    localtime_s(&local, &value);
    std::array<char, 16> stamp{};
    std::strftime(stamp.data(), stamp.size(), "%H:%M:%S", &local);
    return stamp.data();
}
float Scaled(float logical) { return logical * ToolUiScale(); }
// Text at a position, through ImGui's text rendering so logs and tests read it like any label.
void Label(ImVec2 position, ImVec4 color, const std::string& text) {
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::RenderText(position, text.c_str());
    ImGui::PopStyleColor();
}
float LabelWidth(const char* label) { return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2; }

// "1 waiting · 2 working · 1 done", leaving out what is zero.
std::string Summary(const std::vector<Row>& rows) {
    std::string text;
    for (const auto state : {SessionState::Waiting, SessionState::Working, SessionState::Done, SessionState::Exited}) {
        const auto count = std::ranges::count(rows, state, &Row::state);
        if (count == 0) continue;
        if (!text.empty()) text += "  \xC2\xB7  ";
        text += std::format("{} {}", count, state == SessionState::Waiting ? "waiting" : StateLabel(state));
    }
    return text;
}
std::string HolderName(const std::vector<Row>& rows, const EditorAgentReport& report) {
    if (!report.hold) return {};
    if (const auto found = std::ranges::find(rows, report.hold->session, &Row::key); found != rows.end())
        return found->title;
    return report.hold->session.empty() ? OutsideTitle : "Console session " + report.hold->session;
}

// A session as a chip in the row under the header: its state's dot, its name, a lock on the holder. Returns whether it
// was pressed; `menu` asks for its context menu.
bool DrawChip(const Row& row, bool selected, bool& menu) {
    ImGui::PushID(row.id ? row.key.c_str() : "outside");
    const float scale = ToolUiScale();
    const float pad = ToolMetrics::Gap * scale;
    const float dot = ChipDot * scale;
    const float height = ImGui::GetFrameHeight();
    const auto name = ToolClipText(row.title.c_str(), Scaled(ChipTitleMaximum));
    const float lock = row.holds ? ImGui::CalcTextSize(ICON_FA_LOCK).x + ToolMetrics::Space * scale : 0.0f;
    const float width =
        pad + dot * 2 + ToolMetrics::Space * 2 * scale + ImGui::CalcTextSize(name.c_str()).x + lock + pad;
    const auto start = ImGui::GetCursorScreenPos();
    const ImVec2 end{start.x + width, start.y + height};
    const bool pressed = ImGui::InvisibleButton("chip", {width, height});
    const bool hovered = ImGui::IsItemHovered();
    menu = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    auto* draw = ImGui::GetWindowDrawList();
    const bool waiting = row.state == SessionState::Waiting;
    // A waiting agent's chip glows, so it is found without reading
    const auto fill =
        selected  ? ToolPalette::Selected
        : waiting ? EditorTintSurface(ToolPalette::Control, ToolToneColor(ToolTone::Warning), EditorAttentionPulse())
        : hovered ? ToolPalette::Hover
                  : ToolPalette::Control;
    draw->AddRectFilled(start, end, ImGui::GetColorU32(fill), ToolMetrics::Radius * scale);
    const float centerY = start.y + height * 0.5f;
    const auto dotColor = row.state == SessionState::Idle ? ToolPalette::Muted : EditorSessionStateColor(row.state);
    if (row.id)
        draw->AddCircleFilled({start.x + pad + dot, centerY}, dot, ImGui::GetColorU32(dotColor));
    else
        draw->AddCircle({start.x + pad + dot, centerY}, dot, ImGui::GetColorU32(dotColor), 0, 1.5f * scale);
    float x = start.x + pad + dot * 2 + ToolMetrics::Space * 2 * scale;
    const float textY = start.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    const bool exited = row.state == SessionState::Exited;
    Label({x, textY}, exited ? ToolPalette::Disabled : selected ? ToolPalette::Text : ToolPalette::Secondary, name);
    x += ImGui::CalcTextSize(name.c_str()).x + ToolMetrics::Space * scale;
    if (row.holds) Label({x, textY}, EditorAgentColor(), ICON_FA_LOCK);
    if (hovered) {
        auto detail = row.title + "  \xC2\xB7  " + StateLabel(row.state);
        if (!row.activity.empty())
            detail += "\n" + row.activity + (row.activityTime ? "  (" + Age(*row.activityTime) + ")" : "");
        if (row.holds) detail += "\nHolds the editor";
        ToolTooltip(detail.c_str());
    }
    ImGui::PopID();
    return pressed;
}
void DrawTool(const AgentToolUse& tool) {
    const float startX = ImGui::GetCursorPosX();
    ImGui::AlignTextToFramePadding();
    ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
    ImGui::TextColored(ToolPalette::Muted, "%s", Clock(tool.time).c_str());
    ImGui::PopFont();
    ImGui::SameLine(startX + Scaled(TimeColumnWidth));
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ToolPalette::Disabled, "%s", ICON_FA_WRENCH);
    ImGui::SameLine(startX + Scaled(TimeColumnWidth + IconColumnWidth));
    const auto text = ToolClipText(tool.text.c_str(), ImGui::GetContentRegionAvail().x);
    ImGui::AlignTextToFramePadding();
    ImGui::TextColored(ToolPalette::Secondary, "%s", text.c_str());
    if (text != tool.text) ToolTooltip(tool.text.c_str());
}
} // namespace

void AgentsDrawer::Open(std::optional<std::uint64_t> session) {
    _open = true;
    _focusTerminal = true;
    if (session) {
        _selected = std::to_string(*session);
        _view = View::Terminal;
    }
}
void AgentsDrawer::Toggle(std::optional<std::uint64_t> session) {
    if (_open)
        Close();
    else
        Open(session);
}
void AgentsDrawer::Step(int delta) {
    if (_keys.empty()) return;
    const auto current = std::ranges::find(_keys, _selected.value_or(_keys.front()));
    const auto index = current == _keys.end() ? 0 : static_cast<std::ptrdiff_t>(current - _keys.begin());
    const auto count = static_cast<std::ptrdiff_t>(_keys.size());
    _selected = _keys[static_cast<std::size_t>(((index + delta) % count + count) % count)];
    _focusTerminal = !_selected->empty() && _view == View::Terminal;
}
void AgentsDrawer::Pick(std::size_t index) {
    if (index >= _keys.size()) return;
    _selected = _keys[index];
    _focusTerminal = !_selected->empty() && _view == View::Terminal;
}
void AgentsDrawer::SwitchView() {
    _view = _view == View::Terminal ? View::Activity : View::Terminal;
    _focusTerminal = _view == View::Terminal;
}
std::optional<std::uint64_t> AgentsDrawer::ChosenSession() const {
    const auto& key = _selected ? *_selected : (_keys.empty() ? std::string() : _keys.front());
    if (key.empty() || std::ranges::find(_keys, key) == _keys.end()) return std::nullopt;
    return std::stoull(key);
}
std::optional<std::uint64_t> AgentsDrawer::Shown(const std::vector<ConsoleSessionInfo>& sessions) const {
    if (!_open || _view != View::Terminal) return std::nullopt;
    std::vector<const ConsoleSessionInfo*> agents;
    for (const auto& info : sessions)
        if (info.agent) agents.push_back(&info);
    if (agents.empty()) return std::nullopt;
    if (_selected) {
        if (_selected->empty()) return std::nullopt; // The outside row has no terminal
        for (const auto* info : agents)
            if (std::to_string(info->id) == *_selected) return info->id;
    }
    return agents.front()->id;
}

AgentsDrawerRequest AgentsDrawer::Draw(const std::vector<ConsoleSessionInfo>& sessions, const Terminals& terminals,
                                       ConsoleViews& views, const EditorSnapshot& snapshot,
                                       const EditorUiActions& actions, ImVec2 position, ImVec2 size) {
    AgentsDrawerRequest request;
    _terminalFocused = false;
    if (!_open) return request;
    const float scale = ToolUiScale();
    const float margin = Scaled(EdgeMargin);
    // Under the strip at the workspace's top right corner; the bottom-left corner resizes it
    const ImVec2 room{std::max(1.0f, size.x - margin * 2), std::max(1.0f, size.y - margin * 2)};
    const ImVec2 minimum{std::min(Scaled(MinimumWidth), room.x), std::min(Scaled(MinimumHeight), room.y)};
    // A quarter of the screen or so: what a scale or a small window makes larger is capped by the workspace's size
    const float defaultWidth =
        std::min(std::max(Scaled(DefaultWidth), room.x * DefaultWidthShare), room.x * DefaultWidthLimit);
    const float defaultHeight =
        std::min(std::max(Scaled(DefaultHeight), room.y * DefaultHeightShare), room.y * DefaultHeightLimit);
    const float width = std::clamp(_width > 0 ? Scaled(_width) : defaultWidth, minimum.x, room.x);
    const float height = std::clamp(_height > 0 ? Scaled(_height) : defaultHeight, minimum.y, room.y);
    const ImVec2 origin{position.x + size.x - margin - width, position.y + margin};
    ImGui::SetNextWindowPos(origin);
    ImGui::SetNextWindowSize({width, height});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, ToolMetrics::Radius * 2 * scale);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ToolPalette::Panel);
    ImGui::PushStyleColor(ImGuiCol_Border, ToolPalette::Border);
    const bool visible = ImGui::Begin("Agents###editor.agents-drawer", nullptr, DrawerFlags);
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(3);
    if (!visible) {
        ImGui::End();
        return request;
    }
    auto* draw = ImGui::GetWindowDrawList();
    const float pad = Scaled(Padding);
    const float gap = Scaled(ToolMetrics::Gap);

    const auto rows = RowsOf(sessions, snapshot);
    const Row* chosen = Chosen(rows, _selected);
    if (chosen) _selected = chosen->key;
    _keys.clear();
    for (const auto& row : rows)
        _keys.push_back(row.key);
    const bool outside = chosen && !chosen->id;

    // Header: what the agents are doing and who holds the editor
    ImGui::SetCursorScreenPos({origin.x + pad, origin.y + gap});
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Agents");
    ImGui::PopFont();
    const float closeX = origin.x + width - gap - ImGui::GetFrameHeight();
    std::string hold;
    ImVec4 holdColor = EditorAgentColor();
    if (snapshot.agents && !snapshot.agents->control) {
        hold = ICON_FA_BAN "  Agent control off";
        holdColor = ToolToneColor(ToolTone::Warning);
    } else if (snapshot.agents && snapshot.agents->hold) {
        hold = "Editor: " + HolderName(rows, *snapshot.agents);
        if (snapshot.agents->hold->idleSeconds >= 1)
            hold += std::format("  \xC2\xB7  {:.0f} s", snapshot.agents->hold->idleSeconds);
    }
    float headerRight = closeX - gap;
    if (!hold.empty()) {
        const auto clipped = ToolClipText(hold.c_str(), width * 0.4f);
        const float holdWidth = ImGui::CalcTextSize(clipped.c_str()).x;
        ImGui::SetCursorScreenPos({headerRight - holdWidth, origin.y + gap});
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(holdColor, "%s", clipped.c_str());
        if (ImGui::IsItemHovered())
            ToolTooltip(snapshot.agents->control
                            ? "This agent may change and capture the editor now; the others wait. It frees after "
                              "60 s without commands."
                            : "Agent → Agent control is off: agents may only look at the editor.");
        headerRight -= holdWidth + gap;
    }
    const auto summary = Summary(rows);
    if (!summary.empty()) {
        ImGui::SetCursorScreenPos({origin.x + pad + ImGui::CalcTextSize("Agents").x + gap * 1.5f, origin.y + gap});
        const float summaryRoom = headerRight - ImGui::GetCursorScreenPos().x;
        if (summaryRoom > ImGui::GetFontSize() * 3) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ToolPalette::Muted, "%s", ToolClipText(summary.c_str(), summaryRoom).c_str());
        }
    }
    ImGui::SetCursorScreenPos({closeX, origin.y + gap});
    if (ToolIconButton("agents-close", ICON_FA_XMARK, "Close  (Esc, Ctrl+Shift+A)")) _open = false;
    float top = origin.y + gap * 2 + ImGui::GetFrameHeight();
    if (snapshot.agents && !snapshot.agents->visualChecks) {
        ImGui::SetCursorScreenPos({origin.x + pad, top - Scaled(ToolMetrics::Space)});
        ImGui::PushFont(ToolFont(ToolFontRole::Ui), ToolMetrics::CaptionSize);
        ImGui::TextColored(ToolToneColor(ToolTone::Warning), "%s",
                           ICON_FA_EYE_SLASH "  Visual checks off: agents may not capture");
        ImGui::PopFont();
        top += ImGui::GetTextLineHeightWithSpacing();
    }

    // The chips: one per agent, then the button that starts another
    std::optional<std::uint64_t> askClose; // A running agent's close waits for the developer's yes
    if (!rows.empty()) {
        const float rowHeight = ImGui::GetFrameHeight() + Scaled(ToolMetrics::Space) * 2;
        ImGui::SetCursorScreenPos({origin.x + pad, top});
        if (ImGui::BeginChild("agents-chips", {width - pad * 2, rowHeight}, 0,
                              ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoScrollbar)) {
            std::optional<std::string> menuFor;
            for (const auto& row : rows) {
                bool menu = false;
                if (DrawChip(row, &row == chosen, menu)) {
                    _selected = row.key;
                    _focusTerminal = row.id.has_value();
                }
                if (menu) menuFor = row.key;
                ImGui::SameLine(0, Scaled(ToolMetrics::Space) * 1.5f);
            }
            if (menuFor) {
                _selected = *menuFor;
                ImGui::OpenPopup("agent-chip-menu");
            }
            if (ImGui::BeginPopup("agent-chip-menu")) {
                if (const auto* row = Chosen(rows, _selected); row && row->id) {
                    if (ImGui::MenuItem("Open in Console")) request.openConsole = row->id;
                    if (row->state == SessionState::Exited && ImGui::MenuItem("Restart")) request.restart = row->id;
                    if (ImGui::MenuItem("Close session")) {
                        if (row->state == SessionState::Exited)
                            request.close = row->id;
                        else
                            askClose = row->id;
                    }
                }
                ImGui::EndPopup();
            }
            const bool askAdd = std::exchange(_askAdd, false);
            if (ToolIconButton("agents-add", ICON_FA_PLUS, "New agent  (Ctrl+Shift+N)") || askAdd) {
                // From the key, the menu opens under the button rather than at the pointer
                if (askAdd) ImGui::SetNextWindowPos({ImGui::GetItemRectMin().x, ImGui::GetItemRectMax().y});
                ImGui::OpenPopup("agents-add-menu");
            }
            if (ImGui::BeginPopup("agents-add-menu")) {
                for (const auto& kind : AgentKinds) {
                    const auto label = std::string(ICON_FA_ROBOT "  ") +
                                       (kind.id == AgentKind::Custom ? "Custom agent" : kind.sessionName);
                    if (ImGui::MenuItem(label.c_str(), kind.command)) request.add = kind.id;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::EndChild();
        top += rowHeight + Scaled(ToolMetrics::Space);
    }
    draw->AddLine({origin.x, top}, {origin.x + width, top}, ImGui::GetColorU32(ToolPalette::Separator));
    const float bodyHeight = origin.y + height - top;

    if (rows.empty()) {
        // No agent yet: a robot, one line on what happens, and the agents to start, the usual one first; Enter
        // starts that one
        if (std::exchange(_focusTerminal, false)) ImGui::SetWindowFocus();
        _askAdd = false; // Its buttons are the New agent menu
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel) &&
            (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)))
            request.add = _preferred;
        const float buttonWidth = std::min(Scaled(EmptyButtonWidth), width - pad * 2);
        const float buttonHeight = Scaled(ToolMetrics::ControlHeight);
        const float lineHeight = ImGui::GetTextLineHeightWithSpacing();
        const float contentHeight = Scaled(EmptyIconSize) + gap * 2 + lineHeight * 3 + gap * 2 +
                                    (buttonHeight + gap) * static_cast<float>(AgentKinds.size());
        float y = top + std::max(gap, (bodyHeight - contentHeight) * 0.5f);
        const float centerX = origin.x + width * 0.5f;
        ImGui::PushFont(ToolFont(ToolFontRole::Ui), EmptyIconSize);
        const auto iconSize = ImGui::CalcTextSize(ICON_FA_ROBOT);
        Label({centerX - iconSize.x * 0.5f, y}, EditorAgentColor(), ICON_FA_ROBOT);
        ImGui::PopFont();
        y += iconSize.y + gap * 1.5f;
        ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::HeadingSize);
        const char* title = "No agent running";
        Label({centerX - ImGui::CalcTextSize(title).x * 0.5f, y}, ToolPalette::Text, title);
        y += ImGui::GetTextLineHeight() + gap;
        ImGui::PopFont();
        for (const char* line : {"Start one in this project. Its terminal opens here,",
                                 "so you can talk to it while you work in the scene."}) {
            Label({centerX - ImGui::CalcTextSize(line).x * 0.5f, y}, ToolPalette::Muted, line);
            y += lineHeight;
        }
        y += gap * 1.5f;
        std::vector<const AgentKindDefinition*> kinds;
        for (const auto& kind : AgentKinds)
            if (kind.id == _preferred)
                kinds.insert(kinds.begin(), &kind);
            else
                kinds.push_back(&kind);
        for (const auto* kind : kinds) {
            ImGui::SetCursorScreenPos({centerX - buttonWidth * 0.5f, y});
            const auto label = std::string(ICON_FA_ROBOT "  ") +
                               (kind->id == AgentKind::Custom ? "Custom command" : kind->name) + "###agents-start-" +
                               kind->key;
            if (ToolButton(label.c_str(), {buttonWidth, buttonHeight}, kind->id == _preferred)) request.add = kind->id;
            if (ImGui::IsItemHovered()) {
                auto tip = kind->id == AgentKind::Custom
                               ? std::string("Any command line; agent setup asks for it")
                               : std::string("Runs `") + kind->command + "` in the project folder";
                if (kind->id == _preferred) tip += "  (Enter)";
                ToolTooltip(tip.c_str());
            }
            y += buttonHeight + gap;
        }
        ImGui::SetCursorScreenPos({origin.x, y});
        ImGui::Dummy({0, 0});
    } else {
        // The chosen agent: its state, Terminal / Activity, Send scene and its menu
        const bool exited = chosen->state == SessionState::Exited;
        const View view = outside ? View::Activity : _view;
        const float barTop = top + Scaled(ToolMetrics::Space);
        const float more = ImGui::GetFrameHeight();
        const char* send = exited ? "Restart" : "Send scene";
        const float buttons = chosen->id ? more + gap + LabelWidth(send) : 0.0f;
        ImGui::SetCursorScreenPos({origin.x + pad, barTop});
        if (!outside) {
            for (const auto& [option, label] :
                 {std::pair{View::Terminal, "Terminal"}, std::pair{View::Activity, "Activity"}}) {
                if (ToolSegment((std::string(label) + "###agents-view-" + label).c_str(), view == option)) {
                    _view = option;
                    _focusTerminal = option == View::Terminal;
                }
                if (ImGui::IsItemHovered()) ToolTooltip("Terminal / Activity  (Ctrl+Shift+T)");
                ImGui::SameLine();
            }
        }
        const auto state = std::string(StateLabel(chosen->state)) +
                           (chosen->started && !outside ? "  \xC2\xB7  " + Running(*chosen->started) : std::string());
        const float stateRoom = origin.x + width - pad - buttons - gap - ImGui::GetCursorScreenPos().x;
        if (stateRoom > ImGui::GetFontSize() * 3) {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(chosen->state == SessionState::Idle ? ToolPalette::Muted
                                                                   : EditorSessionStateColor(chosen->state),
                               "%s", ToolClipText(state.c_str(), stateRoom).c_str());
        } else {
            ImGui::NewLine();
        }
        if (chosen->id) {
            ImGui::SetCursorScreenPos({origin.x + width - pad - buttons, barTop});
            if (exited) {
                if (ToolButton(send, {}, true)) request.restart = chosen->id;
            } else {
                ImGui::BeginDisabled(!snapshot.ready);
                if (ToolButton(send)) request.sendScene = chosen->id;
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                    ToolTooltip(
                        snapshot.ready
                            ? "Paste the open scene and a capture of it into this agent's prompt  (Ctrl+Shift+S)"
                            : "Run a scene first");
            }
            ImGui::SameLine(0, gap);
            if (ToolIconButton("agent-more", ICON_FA_ELLIPSIS, "More")) ImGui::OpenPopup("agent-more-menu");
            if (ImGui::BeginPopup("agent-more-menu")) {
                if (ImGui::MenuItem("Open in Console")) request.openConsole = chosen->id;
                if (ImGui::MenuItem("Close session")) {
                    if (exited)
                        request.close = chosen->id;
                    else
                        askClose = chosen->id;
                }
                ImGui::EndPopup();
            }
        }
        const float bodyTop = barTop + ImGui::GetFrameHeight() + Scaled(ToolMetrics::Space) * 2;
        // The body stops a grip's height above the bottom, so no child window covers the resize corner
        const ImVec2 body{width - Scaled(1), std::max(1.0f, origin.y + height - bodyTop - Scaled(GripSize))};
        ImGui::SetCursorScreenPos({origin.x + Scaled(1), bodyTop});
        if (view == View::Terminal) {
            if (auto* terminal = terminals ? terminals(*chosen->id) : nullptr) {
                // A dark well with a margin, as deep as the terminal's own background
                const float inset = Scaled(ToolMetrics::Gap);
                ImGui::SetCursorScreenPos({origin.x + inset, bodyTop});
                if (_focusTerminal) ImGui::SetNextWindowFocus();
                ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, ToolMetrics::Radius * scale);
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {Scaled(Padding), Scaled(ToolMetrics::Gap)});
                ImGui::PushStyleColor(ImGuiCol_ChildBg, ToolPalette::Window);
                if (ImGui::BeginChild("agents-terminal", {width - inset * 2, body.y},
                                      ImGuiChildFlags_AlwaysUseWindowPadding, StillChild)) {
                    auto& terminalView = views.View(*chosen->id);
                    terminalView.Draw(*terminal);
                    _terminalFocused = terminalView.Focused();
                }
                ImGui::EndChild();
                ImGui::PopStyleColor();
                ImGui::PopStyleVar(2);
            }
            _focusTerminal = false;
        } else {
            if (ImGui::BeginChild("agents-activity", body, 0, StillChild)) {
                struct Entry {
                    std::chrono::system_clock::time_point time;
                    const EditorControlEvent* event = nullptr;
                    const AgentToolUse* tool = nullptr;
                };
                std::vector<Entry> entries;
                if (_filter != Filter::Tools && snapshot.controlEvents)
                    for (const auto& event : *snapshot.controlEvents)
                        if (event.session == chosen->key) entries.push_back({event.time, &event, nullptr});
                if (_filter != Filter::Editor)
                    if (const auto* report = ReportOf(snapshot, chosen->key))
                        for (const auto& tool : report->tools)
                            entries.push_back({tool.time, nullptr, &tool});
                std::ranges::stable_sort(entries, std::greater{}, &Entry::time);
                ImGui::SetCursorPos({pad, Scaled(ToolMetrics::Space)});
                for (const auto& [filter, label] : {std::pair{Filter::All, "All"}, std::pair{Filter::Editor, "Editor"},
                                                    std::pair{Filter::Tools, "Tools"}}) {
                    if (ToolSegment((std::string(label) + "###agents-filter-" + label).c_str(), _filter == filter))
                        _filter = filter;
                    ImGui::SameLine();
                }
                ImGui::NewLine();
                ImGui::SetCursorPosX(pad);
                if (ImGui::BeginChild("agents-timeline", {body.x - pad * 2, 0})) {
                    if (entries.empty())
                        ImGui::TextColored(ToolPalette::Muted, "%s",
                                           _filter == Filter::Tools
                                               ? "Tools the agent uses appear here (Claude Code hooks)."
                                               : "What the agent does in the editor appears here.");
                    for (std::size_t index = 0; index < entries.size(); ++index) {
                        ImGui::PushID(static_cast<int>(index));
                        if (entries[index].event)
                            DrawEditorAgentEventRow(*entries[index].event, snapshot, actions);
                        else
                            DrawTool(*entries[index].tool);
                        ImGui::PopID();
                    }
                }
                ImGui::EndChild();
            }
            ImGui::EndChild();
        }
    }

    // The bottom-left corner resizes it (it hangs from the top right)
    const float grip = Scaled(GripSize);
    const ImVec2 corner{origin.x, origin.y + height - grip};
    ImGui::SetCursorScreenPos(corner);
    ImGui::InvisibleButton("agents-resize", {grip, grip});
    const bool resizing = ImGui::IsItemActive();
    if (resizing || ImGui::IsItemHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNESW);
        for (int line = 1; line <= 2; ++line) {
            const float inset = grip * 0.3f * static_cast<float>(line);
            draw->AddLine({corner.x + grip * 0.15f, corner.y + inset},
                          {corner.x + grip - inset, corner.y + grip * 0.85f}, ImGui::GetColorU32(ToolPalette::Accent),
                          1.5f * scale);
        }
    }
    if (resizing) {
        const auto delta = ImGui::GetIO().MouseDelta;
        _width = std::clamp(width - delta.x, minimum.x, room.x) / scale;
        _height = std::clamp(height + delta.y, minimum.y, room.y) / scale;
    }

    if (askClose) {
        _confirmClose = askClose;
        ImGui::OpenPopup(CloseAgentDialogId);
    }
    if (const auto closing = DrawCloseAgentDialog(_confirmClose)) request.close = closing;
    ImGui::End();
    return request;
}
