#pragma once
#include <optional>
#include <string>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

// What a Console session is doing, for its icon in the session list (Docs/Editor/Usage.md → Agent workspace):
// Idle: nothing asked of it yet; Working: producing output for a request; Waiting: it asked the developer something
// (rang the bell, or noted "waiting"); Done: finished the last request; Exited: its program ended.
enum class SessionState { Idle, Working, Waiting, Done, Exited };

// Output silent this long ends Working.
inline constexpr double SessionQuietSeconds = 1.5;
// Output this soon after a key or paste is the program echoing the developer's typing, not work.
inline constexpr double SessionEchoSeconds = 1.0;

// Times in seconds on one steady clock; empty when the event has not happened since the program started.
struct SessionSignals {
    double now = 0;
    bool running = true;
    std::optional<double> lastOutput;
    std::optional<double> lastInput; // Developer's keys or pasted text (also text the editor sent)
    std::optional<double> lastBell;
    std::optional<AgentNote> note; // The agent's own report (`ctl note` from this session)
    std::optional<double> noteTime;
};

// A note newer than the developer's last input wins (a bell after it means a question); otherwise a bell after the
// last input means a question, output after the echo window means work, and quiet after work means done. A session
// nobody has typed into is Idle, so a shell's banner or a CLI's start screen shows no tick.
[[nodiscard]] SessionState SessionStateOf(const SessionSignals& signals);
[[nodiscard]] const char* SessionStateName(SessionState state);
// The hover text: the agent's note when it is current, else the terminal title the program set (CLIs put their
// current task there); empty when neither says anything.
[[nodiscard]] std::string SessionActivityText(const SessionSignals& signals, const std::string& terminalTitle);
