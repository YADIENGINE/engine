#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include <algorithm>
#include <format>
#include "ToolUi/ToolTheme.h"

namespace {
// A starting program gets pasted text once its output has been quiet this long, or after the limit.
constexpr auto SettleTime = std::chrono::milliseconds(800);
constexpr auto SettleLimit = std::chrono::seconds(15);
constexpr auto ProcessPollInterval = std::chrono::milliseconds(500);
constexpr const char* SessionEnvironment = "ENGINE_EDITOR_SESSION"; // Read by `ctl` (Tools/commands/ctl.py)
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

ConsoleSessions::Session::Session(std::uint64_t sessionId, std::string sessionTitle, std::optional<AgentKind> agentKind)
    : id(sessionId)
    , title(std::move(sessionTitle))
    , agent(agentKind)
    , launchAgent(agentKind)
    , terminal(ConsolePalette()) {}

void ConsoleSessions::Start(Session& session, const std::string& commandLine) {
    auto environment = _environment;
    environment.emplace_back(SessionEnvironment, std::to_string(session.id));
    session.terminal.Start({.commandLine = commandLine, .directory = _directory, .environment = environment});
    session.started = std::chrono::steady_clock::now();
    session.lastOutput.reset();
    session.lastBell.reset();
    session.outputBytes = 0;
    session.agentObserved = false;
}
std::string ConsoleSessions::NextTitle(const std::string& base) const {
    for (int number = 1;; ++number) {
        auto title = number == 1 ? base : std::format("{} {}", base, number);
        if (std::ranges::none_of(_sessions, [&](const auto& session) { return session->title == title; })) return title;
    }
}
ConsoleSessions::Session* ConsoleSessions::Find(std::uint64_t id) {
    const auto found = std::ranges::find(_sessions, id, [](const auto& session) { return session->id; });
    return found == _sessions.end() ? nullptr : found->get();
}
const ConsoleSessions::Session* ConsoleSessions::Find(std::uint64_t id) const {
    const auto found = std::ranges::find(_sessions, id, [](const auto& session) { return session->id; });
    return found == _sessions.end() ? nullptr : found->get();
}
ConsoleSessions::Session& ConsoleSessions::Add(std::string title, std::optional<AgentKind> agent,
                                               const std::string& commandLine) {
    auto& session = *_sessions.emplace_back(std::make_unique<Session>(_nextId++, std::move(title), agent));
    Start(session, commandLine);
    Select(session.id);
    return session;
}
std::uint64_t ConsoleSessions::OpenShell() { return Add(NextTitle("Shell"), std::nullopt, {}).id; }
std::uint64_t ConsoleSessions::AddAgent(AgentKind kind, const std::string& commandLine) {
    return Add(NextTitle(AgentKindInfo(kind).sessionName), kind, commandLine).id;
}
void ConsoleSessions::Revive(std::uint64_t id, const std::string& commandLine) {
    auto* session = Find(id);
    if (!session) return;
    if (!session->terminal.Running()) {
        if (commandLine.empty())
            session->terminal.Restart();
        else
            Start(*session, commandLine);
    }
    Select(id);
}
void ConsoleSessions::Paste(const std::string& text) {
    if (_sessions.empty()) OpenShell();
    if (auto* session = Find(_selected)) session->terminal.Paste(text);
}
void ConsoleSessions::SendTo(std::uint64_t id, const std::string& text, bool submit) {
    auto* session = Find(id);
    if (!session) return;
    Select(id);
    session->pendingText += text;
    session->pendingSubmit = session->pendingSubmit || submit;
}
void ConsoleSessions::Select(std::uint64_t id) {
    const auto* session = Find(id);
    if (!session) return;
    _selected = id;
    if (session->agent) _lastAgent = id;
}
void ConsoleSessions::Close(std::uint64_t id) {
    const auto found = std::ranges::find(_sessions, id, [](const auto& session) { return session->id; });
    if (found == _sessions.end()) return;
    const auto index = static_cast<std::size_t>(found - _sessions.begin());
    _sessions.erase(found);
    if (_selected != id) return;
    // The row that moved into its place takes the selection; the one above when it was the last
    _selected = _sessions.empty() ? 0 : _sessions[std::min(index, _sessions.size() - 1)]->id;
}
void ConsoleSessions::Update(const EditorAgentReport* report) {
    _report = report;
    const auto now = std::chrono::steady_clock::now();
    const bool pollProcesses = now >= _nextProcessPoll;
    if (pollProcesses) _nextProcessPoll = now + ProcessPollInterval;
    for (auto& pointer : _sessions) {
        auto& session = *pointer;
        session.terminal.Pump();
        if (pollProcesses && session.terminal.Running()) {
            if (const auto processes = session.terminal.ProcessNames()) {
                auto agent = DetectRunningAgent(*processes, session.agent);
                if (!agent && session.launchAgent == AgentKind::Custom) agent = AgentKind::Custom;
                if (agent) session.agentObserved = true;
                if (agent != session.agent) {
                    if (!agent && session.agentObserved) {
                        session.pendingText.clear();
                        session.pendingSubmit = false;
                    }
                    session.title = NextTitle(agent ? AgentKindInfo(*agent).sessionName : "Shell");
                    session.agent = agent;
                    session.started = now; // Old notes and activity belong to the previous application.
                    session.lastBell.reset();
                    if (agent && session.id == _selected) _lastAgent = session.id;
                }
            }
        }
        if (session.terminal.TakeBell()) session.lastBell = now;
        if (session.terminal.OutputBytes() != session.outputBytes) {
            session.outputBytes = session.terminal.OutputBytes();
            session.lastOutput = now;
        }
        if (session.pendingText.empty() && !session.pendingSubmit) continue;
        if (!session.agent) continue; // Never submit queued agent context to a shell prompt.
        const bool settled = session.lastOutput && now - *session.lastOutput >= SettleTime;
        if (!session.terminal.Running() || (!settled && now - session.started < SettleLimit)) continue;
        if (!session.pendingText.empty()) session.terminal.Paste(session.pendingText);
        if (session.pendingSubmit) session.terminal.SendKey(TerminalKey::Enter, {});
        session.pendingText.clear();
        session.pendingSubmit = false;
    }
}
SessionSignals ConsoleSessions::Signals(const Session& session) const {
    const auto input = session.terminal.LastInput();
    SessionSignals signals{.now = Seconds(std::chrono::steady_clock::now()).value_or(0),
                           .running = session.terminal.Running(),
                           .lastOutput = session.lastOutput && *session.lastOutput >= session.started
                                             ? Seconds(session.lastOutput)
                                             : std::nullopt,
                           .lastInput = input && *input >= session.started ? Seconds(input) : std::nullopt,
                           .lastBell = Seconds(session.lastBell)};
    if (!_report || !session.agent) return signals;
    if (const auto found = _report->sessions.find(std::to_string(session.id));
        found != _report->sessions.end() && found->second.note && found->second.noteTime >= session.started) {
        signals.note = found->second.note;
        signals.noteTime = Seconds(found->second.noteTime);
    }
    return signals;
}
ConsoleSessionInfo ConsoleSessions::InfoOf(const Session& session) const {
    const auto signals = Signals(session);
    const auto target = AgentSession();
    return {session.id,
            session.title,
            session.agent,
            SessionStateOf(signals),
            session.agent ? SessionActivityText(signals, session.terminal.Title()) : std::string{},
            session.id == _selected,
            target && *target == session.id,
            session.started,
            session.lastOutput && *session.lastOutput >= session.started ? session.lastOutput : std::nullopt};
}
std::vector<ConsoleSessionInfo> ConsoleSessions::Infos() const {
    std::vector<ConsoleSessionInfo> infos;
    infos.reserve(_sessions.size());
    for (const auto& session : _sessions)
        infos.push_back(InfoOf(*session));
    return infos;
}
std::optional<ConsoleSessionInfo> ConsoleSessions::Info(std::uint64_t id) const {
    const auto* session = Find(id);
    if (!session) return std::nullopt;
    return InfoOf(*session);
}
std::optional<std::uint64_t> ConsoleSessions::Selected() const {
    if (!Find(_selected)) return std::nullopt;
    return _selected;
}
std::optional<std::uint64_t> ConsoleSessions::AgentSession() const {
    if (const auto* selected = Find(_selected); selected && selected->agent) return _selected;
    if (const auto* last = Find(_lastAgent); last && last->agent) return _lastAgent;
    return std::nullopt;
}
TerminalSession* ConsoleSessions::Terminal(std::uint64_t id) {
    auto* session = Find(id);
    return session ? &session->terminal : nullptr;
}
