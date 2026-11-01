#include "Editor/Ui/Panels/Console/ConsolePanel.h"
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
    _selectTab = true;
    if (_sessions[index]->agent) _lastAgent = _sessions[index]->id;
}
void ConsolePanel::Close(std::size_t index) {
    if (index >= _sessions.size()) return;
    _sessions.erase(_sessions.begin() + static_cast<std::ptrdiff_t>(index));
    if (_selected > index || _selected >= _sessions.size()) _selected = _sessions.empty() ? 0 : _selected - 1;
    _selectTab = true;
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
std::optional<ConsoleNewSession> ConsolePanel::Draw() {
    if (_sessions.empty()) OpenShell();
    std::optional<std::size_t> closing;
    std::optional<ConsoleNewSession> request;
    bool showNew = false;
    bool showClose = false;
    const auto sessions = Sessions();
    // Functional minimum; the session list's design lives in the UI layer (Docs/Editor/Usage.md → Agent workspace).
    if (ImGui::BeginTabBar("console-sessions", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (std::size_t index = 0; index < _sessions.size(); ++index) {
            const auto& session = *_sessions[index];
            const auto& info = sessions[index];
            const bool running = info.state != SessionState::Exited;
            const auto label =
                std::format("{} [{}]###console-session-{}", info.title, SessionStateName(info.state), session.id);
            bool open = true;
            const auto flags = ImGuiTabItemFlags_NoTooltip |
                               (session.agent && running ? ImGuiTabItemFlags_NoAssumedClosure : 0) |
                               (_selectTab && index == _selected ? ImGuiTabItemFlags_SetSelected : 0);
            ImGui::PushStyleColor(ImGuiCol_Text, running ? ToolPalette::Text : ToolPalette::Disabled);
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                if (!_selectTab && _selected != index) Select(index);
                ImGui::EndTabItem();
            }
            ImGui::PopStyleColor();
            const auto detail =
                info.title + ": " + (info.activity.empty() ? SessionStateName(info.state) : info.activity);
            ToolTooltip(detail.c_str());
            if (!open) {
                if (session.agent && running) {
                    _confirmClose = index;
                    showClose = true;
                } else {
                    closing = index;
                }
            }
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) showNew = true;
        ToolTooltip("Add terminal");
        ImGui::EndTabBar();
    }
    _selectTab = false;
    if (showNew) ImGui::OpenPopup("Add terminal");
    if (showClose) ImGui::OpenPopup("Close agent session?");
    if (ImGui::BeginPopup("Add terminal")) {
        if (ImGui::MenuItem("Shell")) request = ConsoleNewSession{};
        for (const auto& kind : AgentKinds)
            if (ImGui::MenuItem(kind.sessionName, kind.command)) request = ConsoleNewSession{kind.id};
        ImGui::EndPopup();
    }
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
    if (_sessions.empty()) return request;
    auto& session = *_sessions[_selected];
    session.view.Draw(session.terminal);
    _focused = session.view.Focused();
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
