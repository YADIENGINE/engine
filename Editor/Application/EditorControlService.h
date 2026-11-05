#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <optional>
#include <utility>
#include <string_view>
#include <vector>
#include "Editor/Application/EditorController.h"
#include "Editor/Internals/Agent/AgentReport.h"
#include "Editor/Internals/Agent/ControlLease.h"
#include "Editor/Internals/Platform/Windows/EditorControlPipe.h"
#include "Editor/Internals/Protocol/EditorControlProtocol.h"
#include "Editor/Internals/Session/FrameCapture.h"

// Serves the editor's control endpoint (EditorControlProtocol.h, Docs/Editor/Automation.md) for one session: announces
// the pipe in the build directory and answers requests. `refresh` runs Build & Refresh, or joins the one already
// running, and streams that attempt's log lines and its result. The other commands drive and observe the session the
// way its panels do; a change shows a notice over the viewport. Answers that depend on the worker wait until it has
// applied every earlier command and drawn a frame after that, so a screenshot never shows a stale state.
//
// Several clients are answered at once (EditorControlPipe), each one request at a time. Commands that change or
// capture the editor go through ControlLease: one session holds the editor, and only one such command runs at a time;
// observing commands and notes always answer.

// What the host (the editor window) provides for captures.
struct EditorCaptureHost {
    std::function<std::vector<std::byte>(const ViewportPixels&, const std::vector<FrameCaptureLabel>&)> encodePng;
    // Asks for the window's pixels at the end of the next drawn frame; returns the sequence that capture will carry.
    std::function<std::uint64_t()> requestWindow;
    // The latest window capture when its sequence is at least the requested one, else null.
    std::function<std::shared_ptr<const ViewportPixels>(std::uint64_t)> window;
};

// The host's Audio Preview, for `ctl audio`. `request` queues like a panel click and returns the `handled` count the
// state reaches once it has been carried out.
struct EditorAudioHost {
    std::function<AudioPreviewState()> state;
    std::function<std::uint64_t(const AudioPreviewRequest&)> request;
};

class EditorControlService {
public:
    EditorControlService(const std::filesystem::path& buildDirectory, EditorCaptureHost host);
    ~EditorControlService();
    // The pipe, for tools started by the editor (the console's ENGINE_EDITOR_CONTROL).
    [[nodiscard]] const std::string& Pipe() const { return _pipe.Name(); }
    void Tick(EditorController& controller);
    // Encodes and writes a capture: to `path` (absolute) or, when empty, a new `<prefix>-<time>.png` in the capture
    // folder, which keeps the newest FrameCaptureKeep files. Returns the file; throws when it cannot be written.
    std::filesystem::path WriteCapture(const ViewportPixels& image, const std::vector<FrameCaptureLabel>& labels,
                                       const std::string& path, const std::string& prefix);
    // Off: commands that change the editor are refused (ControlCommandChangesEditor); the developer's switch.
    void SetAgentControl(bool allowed) { _agentControl = allowed; }
    // Off: captures are refused (ControlCommandCaptures); the developer's "Visual checks" switch.
    void SetVisualChecks(bool allowed) { _visualChecks = allowed; }
    void SetAudioHost(EditorAudioHost host) { _audio = std::move(host); }
    // The Console's live sessions, id → title ("Codex 2"), set every frame. Refusals name the holder by its title; a
    // session that leaves this list (closed, or its program exited) gives up the editor.
    void SetConsoleSessions(std::map<std::string, std::string> titles);
    // The developer changed the editor (scene, parameter, playback, ...): the holder loses the editor (ControlLease).
    void DeveloperUsedEditor() { _lease.DeveloperUsed(); }
    // Notes, tool lines and who holds the editor, updated every Tick.
    [[nodiscard]] const EditorAgentReport& Report() const { return _report; }

private:
    // The client a request came from: its connection, its session, and whether the command needs the hold.
    struct Requester {
        std::uint64_t client = 0;
        std::string session;
        bool holds = false;
    };
    struct Watch {
        Requester from;
        std::uint64_t attempt = 0;
        std::uint64_t sentSequence = 0;
    };
    // An answer that waits for the session: `poll` returns it once ready, throws to fail, or is abandoned at the
    // deadline.
    struct Pending {
        Requester from;
        std::chrono::steady_clock::time_point deadline;
        std::string waitingFor;
        std::function<std::optional<nlohmann::json>(EditorController&)> poll;
    };
    using Answer = std::function<nlohmann::json(EditorController&)>;
    void Handle(const EditorControlPipe::Line& line, EditorController& controller);
    // Refuses a command that needs the hold when another such command runs or another session holds the editor.
    void TakeHold(Requester& from);
    // How a refusal or the status names a session: its Console title, or what a client outside the Console is.
    [[nodiscard]] std::string NameOf(const std::string& session) const;
    [[nodiscard]] bool Busy(std::uint64_t client) const;
    void Note(const EditorControlRequest& request, EditorController& controller);
    void UpdateReport();
    // Returns the immediate answer, or nothing when the command left a Pending.
    [[nodiscard]] std::optional<nlohmann::json> Run(const EditorControlRequest& request, EditorController& controller);
    void Refresh(EditorController& controller);
    void Await(std::string waitingFor, std::chrono::steady_clock::duration timeout,
               std::function<std::optional<nlohmann::json>(EditorController&)> poll);
    // Waits for the worker to confirm everything sent so far and draw a frame after it, then answers.
    void AwaitWorker(EditorController& controller, std::chrono::steady_clock::duration timeout, Answer answer);
    void AwaitReady(const std::string& id);
    [[nodiscard]] nlohmann::json Screenshot(const nlohmann::json& arguments, EditorController& controller);
    [[nodiscard]] nlohmann::json AudioStatus(const EditorController& controller) const;
    void AudioControl(const nlohmann::json& arguments);
    // The whole editor window after its next frame (no scene needed).
    void WindowShot(const nlohmann::json& arguments);
    // Seeks to evenly spaced times one after another and answers with one labelled grid image.
    void Frames(const nlohmann::json& arguments, EditorController& controller);
    [[nodiscard]] static nlohmann::json Saved(const std::filesystem::path& file, const ViewportPixels& image,
                                              EditorController& controller);
    void Stream(EditorController& controller);
    void Poll(EditorController& controller);
    EditorControlPipe _pipe;
    std::filesystem::path _announcement;
    std::filesystem::path _captures;
    EditorCaptureHost _host;
    EditorAudioHost _audio;
    std::optional<Watch> _watch;               // At most one refresh streams at a time
    std::map<std::uint64_t, Pending> _pending; // By client
    Requester _current;                        // The request being run (Handle) or answered (Poll)
    ControlLease _lease;
    std::map<std::string, std::string> _sessionTitles;
    EditorAgentReport _report;
    bool _agentControl = true;
    bool _visualChecks = true;

public:
    EditorControlService(const EditorControlService&) = delete;
    EditorControlService& operator=(const EditorControlService&) = delete;
};
