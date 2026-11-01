#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include <algorithm>
#include <cstdint>
#include <format>
#include "ToolUi/ToolTheme.h"

namespace {
// A starting agent gets pasted text once its output has been quiet this long, or after the limit.
constexpr auto SettleTime = std::chrono::milliseconds(800);
constexpr auto SettleLimit = std::chrono::seconds(15);
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
} // namespace

// Editörün kendi kısayolları konsol odaktayken de çalışır: alt alan, konsol, komut paleti.
ConsolePanel::Session::Session(std::string sessionTitle, bool agentSession)
    : title(std::move(sessionTitle))
    , agent(agentSession)
    , terminal(ConsolePalette())
    , view({ImGuiMod_Ctrl | ImGuiKey_J, ImGuiMod_Ctrl | ImGuiKey_GraveAccent,
            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P}) {}

void ConsolePanel::Start(Session& session, const std::string& commandLine) {
    session.terminal.Start({.commandLine = commandLine, .directory = _directory, .environment = _environment});
    session.started = session.lastOutput = std::chrono::steady_clock::now();
    session.outputBytes = 0;
}
ConsolePanel::Session& ConsolePanel::Add(std::string title, bool agent, const std::string& commandLine) {
    auto& session = *_sessions.emplace_back(std::make_unique<Session>(std::move(title), agent));
    Start(session, commandLine);
    Select(_sessions.size() - 1);
    return session;
}
void ConsolePanel::OpenShell() {
    const auto shells = std::ranges::count_if(_sessions, [](const auto& session) { return !session->agent; });
    Add(shells ? std::format("Shell {}", shells + 1) : std::string("Shell"), false, {});
}
void ConsolePanel::OpenAgent(const std::string& title, const std::string& commandLine) {
    if (const auto index = AgentSession()) {
        auto& session = *_sessions[*index];
        session.title = title;
        if (!session.terminal.Running()) Start(session, commandLine);
        Select(*index);
        return;
    }
    Add(title, true, commandLine);
}
void ConsolePanel::Paste(const std::string& text) {
    if (_sessions.empty()) OpenShell();
    _sessions[_selected]->terminal.Paste(text);
}
void ConsolePanel::SendToAgent(const std::string& title, const std::string& commandLine, const std::string& text,
                               bool submit) {
    OpenAgent(title, commandLine);
    auto& session = *_sessions[_selected];
    session.pendingText += text;
    session.pendingSubmit = session.pendingSubmit || submit;
}
void ConsolePanel::Select(std::size_t index) {
    if (index >= _sessions.size()) return;
    _selected = index;
    _selectTab = true;
}
void ConsolePanel::Close(std::size_t index) {
    if (index >= _sessions.size()) return;
    _sessions.erase(_sessions.begin() + static_cast<std::ptrdiff_t>(index));
    if (_selected > index || _selected >= _sessions.size()) _selected = _sessions.empty() ? 0 : _selected - 1;
    _selectTab = true;
}
void ConsolePanel::Update() {
    const auto now = std::chrono::steady_clock::now();
    const bool looking = _focused; // Drawn with the keyboard last frame; a hidden panel draws nothing
    _focused = false;
    for (std::size_t index = 0; index < _sessions.size(); ++index) {
        auto& session = *_sessions[index];
        session.terminal.Pump();
        if (session.terminal.TakeBell() && !(index == _selected && looking)) session.attention = true;
        if (session.terminal.OutputBytes() != session.outputBytes) {
            session.outputBytes = session.terminal.OutputBytes();
            session.lastOutput = now;
        }
        if (session.pendingText.empty() && !session.pendingSubmit) continue;
        const bool settled = session.outputBytes > 0 && now - session.lastOutput >= SettleTime;
        if (!session.terminal.Running() || (!settled && now - session.started < SettleLimit)) continue;
        if (!session.pendingText.empty()) session.terminal.Paste(session.pendingText);
        if (session.pendingSubmit) session.terminal.SendKey(TerminalKey::Enter, {});
        session.pendingText.clear();
        session.pendingSubmit = false;
    }
}
void ConsolePanel::Draw() {
    if (_sessions.empty()) OpenShell();
    std::optional<std::size_t> closing;
    if (ImGui::BeginTabBar("console-sessions", ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (std::size_t index = 0; index < _sessions.size(); ++index) {
            auto& session = *_sessions[index];
            const auto label = std::format("{}{}###console-session-{}", session.title, session.attention ? " *" : "",
                                           static_cast<const void*>(&session));
            bool open = true;
            const auto flags = _selectTab && index == _selected ? ImGuiTabItemFlags_SetSelected : 0;
            if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
                if (!_selectTab) _selected = index;
                ImGui::EndTabItem();
            }
            if (!open) closing = index;
        }
        if (ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing | ImGuiTabItemFlags_NoTooltip)) OpenShell();
        ImGui::EndTabBar();
    }
    _selectTab = false;
    if (closing) Close(*closing);
    if (_sessions.empty()) {
        _focused = false;
        return;
    }
    auto& session = *_sessions[_selected];
    session.view.Draw(session.terminal);
    _focused = session.view.Focused();
    if (_focused) session.attention = false;
}
bool ConsolePanel::Focused() const { return _focused; }
std::vector<ConsoleSessionInfo> ConsolePanel::Sessions() const {
    std::vector<ConsoleSessionInfo> sessions;
    for (const auto& session : _sessions)
        sessions.push_back({session->title, session->agent, session->terminal.Running(), session->attention});
    return sessions;
}
std::optional<std::size_t> ConsolePanel::AgentSession() const {
    const auto found = std::ranges::find_if(_sessions, [](const auto& session) { return session->agent; });
    if (found == _sessions.end()) return std::nullopt;
    return static_cast<std::size_t>(found - _sessions.begin());
}
