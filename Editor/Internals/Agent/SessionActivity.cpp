#include "Editor/Internals/Agent/SessionActivity.h"

namespace {
bool After(const std::optional<double>& time, const std::optional<double>& other) {
    return time && (!other || *time > *other);
}
bool NoteCurrent(const SessionSignals& signals) {
    return signals.note && signals.noteTime && !After(signals.lastInput, signals.noteTime);
}
std::string Trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}
} // namespace

SessionState SessionStateOf(const SessionSignals& signals) {
    if (!signals.running) return SessionState::Exited;
    if (NoteCurrent(signals)) {
        if (After(signals.lastBell, signals.noteTime)) return SessionState::Waiting;
        switch (signals.note->state) {
        case AgentActivityState::Working: return SessionState::Working;
        case AgentActivityState::Waiting: return SessionState::Waiting;
        case AgentActivityState::Idle   : return SessionState::Done;
        }
    }
    if (!signals.lastInput) return SessionState::Idle;
    if (After(signals.lastBell, signals.lastInput)) return SessionState::Waiting;
    const bool worked = signals.lastOutput && *signals.lastOutput - *signals.lastInput > SessionEchoSeconds;
    if (!worked) return SessionState::Idle;
    return signals.now - *signals.lastOutput < SessionQuietSeconds ? SessionState::Working : SessionState::Done;
}
const char* SessionStateName(SessionState state) {
    switch (state) {
    case SessionState::Idle   : return "idle";
    case SessionState::Working: return "working";
    case SessionState::Waiting: return "waiting for you";
    case SessionState::Done   : return "done";
    case SessionState::Exited : return "exited";
    }
    return "idle";
}
std::string SessionActivityText(const SessionSignals& signals, const std::string& terminalTitle) {
    if (NoteCurrent(signals) && !signals.note->text.empty()) return signals.note->text;
    return Trimmed(terminalTitle);
}
std::string AgentHeadline(SessionState state, const std::string& activity, const std::string& lastEvent) {
    if (state == SessionState::Waiting) return "Waiting for you";
    if (state == SessionState::Working)
        return !activity.empty() ? activity : !lastEvent.empty() ? lastEvent : "Working";
    if (!lastEvent.empty()) return lastEvent;
    return state == SessionState::Done ? "Done" : "No agent activity";
}
std::string AgentEarlierLabel(std::size_t total) {
    constexpr std::size_t Shown = 99;
    if (total <= 1) return {};
    return total - 1 > Shown ? std::to_string(Shown) + "+" : "+" + std::to_string(total - 1);
}
std::string AgentAgeLabel(double seconds) {
    constexpr double Recent = 60;
    constexpr double Hour = 3600;
    if (seconds < Recent) return {};
    if (seconds < Hour) return std::to_string(static_cast<int>(seconds / Recent)) + "m";
    return std::to_string(static_cast<int>(seconds / Hour)) + "h";
}
