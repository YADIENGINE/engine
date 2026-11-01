#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui_internal.h>
#include <algorithm>
#include <format>
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
// A starting program gets pasted text once its output has been quiet this long, or after the limit.
constexpr auto SettleTime = std::chrono::milliseconds(800);
constexpr auto SettleLimit = std::chrono::seconds(15);
constexpr const char* SessionEnvironment = "ENGINE_EDITOR_SESSION"; // Read by `ctl note` (Tools/commands/ctl.py)
std::uint32_t ToRgb(const ImVec4& color) {
    const auto channel = [](float value) { return static_cast<std::uint32_t>(value * 255.0f + 0.5f) & 0xFF; };
    return (channel(color.x) << 16) | (channel(color.y) << 8) | channel(color.z);
}
TerminalPalette ConsolePalette() {
    TerminalPalette palette;
    palette.background = ToRgb(ToolPalette::Window);
    palette.foreground = ToRgb(ToolPalette::Text);
    return palette;
}
std::optional<double> Seconds(const std::optional<std::chrono::steady_clock::time_point>& time) {
    if (!time) return std::nullopt;
    return std::chrono::duration<double>(time->time_since_epoch()).count();
}
} // namespace

// Editor shortcuts stay available while the terminal owns keyboard input.
ConsolePanel::Session::Session(std::uint64_t sessionId, std::string sessionTitle, std::optional<AgentKind> agentKind)
    : id(sessionId)
    , title(std::move(sessionTitle))
    , agent(agentKind)
    , terminal(ConsolePalette())
    , view({ImGuiMod_Ctrl | ImGuiKey_J, ImGuiMod_Ctrl | ImGuiKey_GraveAccent,
            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A}) {}

void ConsolePanel::Start(Session& session, const std::string& commandLine) {
    auto environment = _environment;
    environment.emplace_back(SessionEnvironment, std::to_string(session.id));
    session.terminal.Start({.commandLine = commandLine, .directory = _directory, .environment = environment});
    session.started = std::chrono::steady_clock::now();
    session.lastOutput.reset();
    session.lastBell.reset();
    session.outputBytes = 0;
}
std::string ConsolePanel::NextTitle(const std::string& base) const {
    for (int number = 1;; ++number) {
        auto title = number == 1 ? base : std::format("{} {}", base, number);
        if (std::ranges::none_of(_sessions, [&](const auto& session) { return session->title == title; })) return title;
    }
}
ConsolePanel::Session& ConsolePanel::Add(std::string title, std::optional<AgentKind> agent,
                                         const std::string& commandLine) {
    auto& session = *_sessions.emplace_back(std::make_unique<Session>(_nextId++, std::move(title), agent));
    Start(session, commandLine);
    Select(_sessions.size() - 1);
    return session;
}
void ConsolePanel::OpenShell() { Add(NextTitle("Shell"), std::nullopt, {}); }
void ConsolePanel::AddAgent(AgentKind kind, const std::string& commandLine) {
    Add(NextTitle(AgentKindInfo(kind).sessionName), kind, commandLine);
}
void ConsolePanel::Revive(std::size_t index, const std::string& commandLine) {
    if (index >= _sessions.size()) return;
    auto& session = *_sessions[index];
    if (!session.terminal.Running()) {
        if (commandLine.empty())
            session.terminal.Restart();
        else
            Start(session, commandLine);
    }
    Select(index);
}
void ConsolePanel::Paste(const std::string& text) {
    if (_sessions.empty()) OpenShell();
    _sessions[_selected]->terminal.Paste(text);
}
void ConsolePanel::SendTo(std::size_t index, const std::string& text, bool submit) {
    if (index >= _sessions.size()) return;
    Select(index);
    auto& session = *_sessions[index];
    session.pendingText += text;
    session.pendingSubmit = session.pendingSubmit || submit;
}
void ConsolePanel::Select(std::size_t index) {
    if (index >= _sessions.size()) return;
    _selected = index;
    if (_sessions[index]->agent) _lastAgent = _sessions[index]->id;
}
void ConsolePanel::Close(std::size_t index) {
    if (index >= _sessions.size()) return;
    _sessions.erase(_sessions.begin() + static_cast<std::ptrdiff_t>(index));
    if (_selected > index || _selected >= _sessions.size()) _selected = _sessions.empty() ? 0 : _selected - 1;
}
void ConsolePanel::Update() {
    const auto now = std::chrono::steady_clock::now();
    _focused = false; // Set again by Draw; a hidden panel draws nothing
    for (auto& pointer : _sessions) {
        auto& session = *pointer;
        session.terminal.Pump();
        if (session.terminal.TakeBell()) session.lastBell = now;
        if (session.terminal.OutputBytes() != session.outputBytes) {
            session.outputBytes = session.terminal.OutputBytes();
            session.lastOutput = now;
        }
        if (session.pendingText.empty() && !session.pendingSubmit) continue;
        const bool settled = session.lastOutput && now - *session.lastOutput >= SettleTime;
        if (!session.terminal.Running() || (!settled && now - session.started < SettleLimit)) continue;
        if (!session.pendingText.empty()) session.terminal.Paste(session.pendingText);
        if (session.pendingSubmit) session.terminal.SendKey(TerminalKey::Enter, {});
        session.pendingText.clear();
        session.pendingSubmit = false;
    }
}
SessionSignals ConsolePanel::Signals(const Session& session) const {
    SessionSignals signals{.now = Seconds(std::chrono::steady_clock::now()).value_or(0),
                           .running = session.terminal.Running(),
                           .lastOutput = Seconds(session.lastOutput),
                           .lastInput = Seconds(session.terminal.LastInput()),
                           .lastBell = Seconds(session.lastBell)};
    if (const auto note = _notes.find(std::to_string(session.id));
        note != _notes.end() && note->second.time >= session.started) { // Not from before a restart
        signals.note = note->second.note;
        signals.noteTime = Seconds(note->second.time);
    }
    return signals;
}
bool ConsolePanel::DrawSessionRow(std::size_t index, const ConsoleSessionInfo& info) {
    ImGui::PushID(std::to_string(_sessions[index]->id).c_str());
    const auto start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();
    const float scale = ToolUiScale();
    const float gap = ToolMetrics::Space * scale;
    const float iconWidth = ImGui::GetFontSize();
    const ImVec2 end{start.x + width, start.y + height};
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                         ImGui::IsMouseHoveringRect(start, end);
    auto* draw = ImGui::GetWindowDrawList();
    if (info.selected || hovered)
        draw->AddRectFilled(start, end, ImGui::GetColorU32(info.selected ? ToolPalette::Selected : ToolPalette::Hover),
                            ToolMetrics::Radius * ToolUiScale());
    if (info.selected)
        draw->AddRectFilled({start.x + scale, start.y + gap}, {start.x + 3 * scale, end.y - gap},
                            ImGui::GetColorU32(ToolPalette::Accent), scale);
    // State and close share one trailing slot; closing a row does not also select it.
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, ImGui::GetStyle().ItemSpacing.y});
    if (ImGui::Selectable("##session", info.selected, 0, {std::max(1.0f, width - height), height})) Select(index);
    ImGui::PopStyleColor(3);
    const auto detail = info.title + "\n" + SessionStateName(info.state) +
                        (info.activity.empty() ? std::string() : "\n" + info.activity);
    ToolTooltip(detail.c_str());
    const bool exited = info.state == SessionState::Exited;
    const auto textColor = exited ? ToolPalette::Disabled : ToolPalette::Text;
    const auto kindColor = exited          ? ToolPalette::Disabled
                           : info.selected ? ToolPalette::Accent
                                           : ToolPalette::Secondary;
    const float textY = start.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    draw->AddText({start.x + gap * 2, textY}, ImGui::GetColorU32(kindColor),
                  info.agent ? ICON_FA_ROBOT : ICON_FA_TERMINAL);
    const float trailingX = end.x - height;
    const float titleX = start.x + iconWidth + gap * 3;
    const float titleEnd = trailingX - gap;
    if (titleEnd > titleX) {
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::RenderTextEllipsis(draw, {titleX, textY}, {titleEnd, textY + ImGui::GetTextLineHeight()}, titleEnd,
                                  info.title.c_str(), nullptr, nullptr);
        ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 0);
    bool close = false;
    if (hovered || ImGui::IsItemFocused()) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::Secondary);
        close = ToolIconButton("close", ICON_FA_XMARK, "Close session");
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    } else {
        ImGui::Dummy({height, height});
        const auto* icon = EditorSessionStateIcon(info.state);
        draw->AddText({trailingX + (height - ImGui::CalcTextSize(icon).x) * 0.5f, textY},
                      ImGui::GetColorU32(EditorSessionStateColor(info.state)), icon);
    }
    ImGui::PopStyleVar();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("session-actions");
    if (ImGui::BeginPopup("session-actions")) {
        close = ImGui::MenuItem("Close") || close;
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return close;
}
std::optional<ConsoleNewSession> ConsolePanel::Draw() {
    if (_sessions.empty()) OpenShell();
    _focused = false;
    std::optional<std::size_t> closing;
    std::optional<ConsoleNewSession> request;
    bool showClose = false;
    const auto sessions = Sessions();
    const auto origin = ImGui::GetCursorPos();
    const auto originScreen = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const float scale = ToolUiScale();
    const float fontSize = ImGui::GetFontSize();
    const float splitterWidth = ToolMetrics::Space * scale;
    const float maxWidth = std::max(1.0f, available.x - splitterWidth - fontSize * 6);
    const float minWidth = std::min(fontSize * 7, maxWidth);
    if (_sessionListWidth <= 0) _sessionListWidth = minWidth / scale;
    float listWidth = std::clamp(_sessionListWidth * scale, minWidth, maxWidth);
    float terminalWidth = std::max(1.0f, available.x - listWidth - splitterWidth);
    ImGui::SetCursorPos({origin.x + terminalWidth, origin.y});
    ImGui::InvisibleButton("console-session-splitter", {splitterWidth, std::max(1.0f, available.y)});
    const bool resizing = ImGui::IsItemActive();
    const bool resizeHovered = ImGui::IsItemHovered();
    if (resizing || resizeHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (resizing) {
        listWidth = std::clamp(listWidth - ImGui::GetIO().MouseDelta.x, minWidth, maxWidth);
        _sessionListWidth = listWidth / scale;
        terminalWidth = std::max(1.0f, available.x - listWidth - splitterWidth);
    }
    const float splitterX = originScreen.x + terminalWidth + splitterWidth * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        {splitterX, originScreen.y}, {splitterX, originScreen.y + available.y},
        ImGui::GetColorU32(resizing || resizeHovered ? ToolPalette::Accent : ToolPalette::Separator));
    ImGui::SetCursorPos({origin.x + terminalWidth + splitterWidth, origin.y});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    if (BeginToolPanel("console-session-list", {listWidth, available.y}, ToolSurface::Utility, 0,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, ToolMetrics::Space)) {
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        auto addBorder = ToolPalette::Border;
        addBorder.w *= 0.45f;
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, scale);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, ToolMetrics::Radius * scale);
        ImGui::PushStyleColor(ImGuiCol_Border, addBorder);
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Accent));
        const bool add = ToolButton(ICON_FA_PLUS "###console-add-terminal",
                                    {buttonWidth, EditorToolbarMetrics::ControlHeight * scale});
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        ToolTooltip("Add terminal");
        if (add) ImGui::OpenPopup("Add terminal");
        if (ImGui::BeginPopup("Add terminal")) {
            if (ImGui::MenuItem(ICON_FA_TERMINAL "  Shell")) request = ConsoleNewSession{};
            for (const auto& kind : AgentKinds) {
                const auto label = std::string(ICON_FA_ROBOT "  ") +
                                   (kind.id == AgentKind::Custom ? "Custom agent" : kind.sessionName);
                if (ImGui::MenuItem(label.c_str(), kind.command)) request = ConsoleNewSession{kind.id};
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        if (ImGui::BeginChild("sessions")) {
            for (std::size_t index = 0; index < sessions.size(); ++index) {
                if (DrawSessionRow(index, sessions[index])) {
                    if (sessions[index].agent && sessions[index].state != SessionState::Exited) {
                        _confirmClose = index;
                        showClose = true;
                    } else {
                        closing = index;
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    EndToolPanel();
    ImGui::PopStyleVar(2);
    if (showClose) ImGui::OpenPopup("Close agent session?");
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({380 * ToolUiScale(), 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Close agent session?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Closing this session ends its running program. Any work in progress will be interrupted.");
        ImGui::Separator();
        if (ToolButton("Close session", {}, true)) {
            closing = _confirmClose;
            _confirmClose.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ToolButton("Keep open") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            _confirmClose.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
    if (closing) Close(*closing);
    ImGui::SetCursorPos(origin);
    if (ImGui::BeginChild("console-terminal", {terminalWidth, available.y}, ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) &&
        !_sessions.empty()) {
        auto& session = *_sessions[_selected];
        session.view.Draw(session.terminal);
        _focused = session.view.Focused();
    }
    ImGui::EndChild();
    return request;
}
bool ConsolePanel::Focused() const { return _focused; }
std::vector<ConsoleSessionInfo> ConsolePanel::Sessions() const {
    std::vector<ConsoleSessionInfo> sessions;
    for (std::size_t index = 0; index < _sessions.size(); ++index) {
        const auto& session = *_sessions[index];
        const auto signals = Signals(session);
        sessions.push_back({session.title, session.agent, SessionStateOf(signals),
                            SessionActivityText(signals, session.terminal.Title()), index == _selected});
    }
    return sessions;
}
std::optional<std::size_t> ConsolePanel::AgentSession() const {
    if (_selected < _sessions.size() && _sessions[_selected]->agent) return _selected;
    const auto found = std::ranges::find_if(_sessions, [&](const auto& session) { return session->id == _lastAgent; });
    if (found == _sessions.end()) return std::nullopt;
    return static_cast<std::size_t>(found - _sessions.begin());
}
