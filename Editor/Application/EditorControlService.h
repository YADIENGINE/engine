#pragma once
#include <nlohmann/json.hpp>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>
#include <vector>
#include "Editor/Application/EditorController.h"
#include "Editor/Internals/Platform/Windows/EditorControlPipe.h"
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

// Serves the editor's control endpoint (EditorControlProtocol.h, Docs/Editor/Automation.md) for one session: announces
// the pipe in the build directory and answers requests. `refresh` runs Build & Refresh, or joins the one already
// running, and streams that attempt's log lines and its result. The other commands drive and observe the session the
// way its panels do; a change shows a notice over the viewport. Answers that depend on the worker wait until it has
// applied every earlier command and drawn a frame after that, so a screenshot never shows a stale state.
class EditorControlService {
public:
    using PngEncoder = std::function<std::vector<std::byte>(const PreviewPixels&)>;
    EditorControlService(const std::filesystem::path& buildDirectory, PngEncoder encodePng);
    ~EditorControlService();
    // The pipe, for tools started by the editor (the console's ENGINE_EDITOR_CONTROL).
    [[nodiscard]] const std::string& Pipe() const { return _pipe.Name(); }
    void Tick(EditorController& controller);

private:
    struct Watch {
        std::uint64_t client = 0;
        std::uint64_t attempt = 0;
        std::uint64_t sentSequence = 0;
    };
    // An answer that waits for the session: `poll` returns it once ready, throws to fail, or is abandoned at the
    // deadline.
    struct Pending {
        std::uint64_t client = 0;
        std::chrono::steady_clock::time_point deadline;
        std::string waitingFor;
        std::function<std::optional<nlohmann::json>(EditorController&)> poll;
    };
    using Answer = std::function<nlohmann::json(EditorController&)>;
    void Handle(std::string_view line, EditorController& controller);
    // Returns the immediate answer, or nothing when the command left a Pending.
    [[nodiscard]] std::optional<nlohmann::json> Run(const EditorControlRequest& request, EditorController& controller);
    void Refresh(EditorController& controller);
    void Await(std::string waitingFor, std::chrono::steady_clock::duration timeout,
               std::function<std::optional<nlohmann::json>(EditorController&)> poll);
    // Waits for the worker to confirm everything sent so far and draw a frame after it, then answers.
    void AwaitWorker(EditorController& controller, std::chrono::steady_clock::duration timeout, Answer answer);
    void AwaitReady(const std::string& id);
    [[nodiscard]] nlohmann::json Screenshot(const nlohmann::json& arguments, EditorController& controller);
    void Stream(EditorController& controller);
    void Poll(EditorController& controller);
    EditorControlPipe _pipe;
    std::filesystem::path _announcement;
    std::filesystem::path _captures;
    PngEncoder _encodePng;
    std::optional<Watch> _watch;
    std::optional<Pending> _pending;

public:
    EditorControlService(const EditorControlService&) = delete;
    EditorControlService& operator=(const EditorControlService&) = delete;
};
