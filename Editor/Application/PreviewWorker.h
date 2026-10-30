#pragma once
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Platform/Windows/ChildProcess.h"
#include "Editor/Internals/Platform/Windows/ControlChannel.h"
#include "Editor/Internals/Platform/Windows/SharedFrames.h"

// Catalog: reports the catalog and exits after the acknowledgement. Preview: opens a preview and streams frames.
enum class PreviewWorkerMode { Catalog, Preview };

// One worker process started from a snapshot, with its own session (control channel + frame transport) and the
// startup / shutdown deadlines. Several can live at once: the running preview, the one replacing it, and retired ones
// still shutting down. Session state (generation, parameters, readiness) belongs to the controller.
class PreviewWorker {
public:
    PreviewWorker(const ProcessCommand& command, PreviewWorkerMode mode);
    ~PreviewWorker();
    [[nodiscard]] PreviewWorkerMode Mode() const { return _mode; }
    [[nodiscard]] bool Running() const { return _process.Running(); }
    [[nodiscard]] std::optional<unsigned long> ExitCode() const { return _process.ExitCode(); }
    [[nodiscard]] std::vector<std::string> DrainOutput() { return _process.DrainOutput(); }
    [[nodiscard]] std::vector<PreviewMessage> Poll();
    [[nodiscard]] std::optional<PreviewPixels> ReadFrame(std::uint64_t generation);
    void Send(std::uint64_t generation, std::uint64_t request, const std::string& type, nlohmann::json data);
    // The peer closed the control connection while its process keeps running.
    [[nodiscard]] bool ConnectionLost();
    // The startup clock runs from launch and again after a reset, until the preview reports ready.
    void AwaitReady();
    void MarkReady() { _startupDeadline.reset(); }
    [[nodiscard]] bool StartupTimedOut() const;
    // Asks the worker to quit; EnforceShutdown terminates its process tree once the grace period is over.
    void Shutdown();
    [[nodiscard]] bool ShuttingDown() const { return _shutdownDeadline.has_value(); }
    // Returns true when this call had to terminate the worker.
    bool EnforceShutdown();
    void Terminate() { _process.Terminate(); }

private:
    PreviewWorkerMode _mode;
    ChildProcess _process;
    std::unique_ptr<ControlChannel> _channel;
    std::unique_ptr<SharedFrames> _frames;
    std::optional<std::chrono::steady_clock::time_point> _startupDeadline;
    std::optional<std::chrono::steady_clock::time_point> _shutdownDeadline;
    std::optional<std::chrono::steady_clock::time_point> _closedAt;
    bool _terminated = false;

public:
    PreviewWorker(const PreviewWorker&) = delete;
    PreviewWorker& operator=(const PreviewWorker&) = delete;
};
