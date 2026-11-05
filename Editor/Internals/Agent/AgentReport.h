#pragma once
#include <chrono>
#include <cstddef>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

// What the control endpoint knows about agents, for the Console, the toolbar strip and the Agents panel
// (EditorControlService::Report, EditorSnapshot::agents). Sessions are keyed by ENGINE_EDITOR_SESSION: a Console
// session's id as text, "" for every client outside the Console.

// A tool an agent CLI used, from its hook (`ctl note --hook --tool-stdin`): "Edit Player.cpp", "Bash Run the tests".
struct AgentToolUse {
    std::chrono::system_clock::time_point time;
    std::string text;
};
inline constexpr std::size_t AgentToolsKept = 100; // Per session; the oldest leave first

struct AgentSessionReport {
    std::optional<AgentNote> note; // Its latest `ctl note`, also a live tool line
    std::chrono::steady_clock::time_point noteTime;
    std::deque<AgentToolUse> tools; // Oldest first
    std::size_t toolCount = 0;      // Every tool this editor session, also those that left `tools`
};

// The session that may change or capture the editor now (ControlLease).
struct AgentEditorHold {
    std::string session;
    double idleSeconds = 0.0; // Since its last command; 0 while one is being answered
};

struct EditorAgentReport {
    std::map<std::string, AgentSessionReport> sessions;
    std::optional<AgentEditorHold> hold; // Empty: nobody holds the editor
    bool control = true;                 // The developer's Agent control switch
    bool visualChecks = true;            // The developer's Visual checks switch
};
