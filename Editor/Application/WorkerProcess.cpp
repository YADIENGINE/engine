#include "Editor/Application/WorkerProcess.h"
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
constexpr auto StartupLimit = std::chrono::seconds(30); // Until the first frame (Scene) or the catalog exit
constexpr auto ShutdownGrace = std::chrono::seconds(5);
// The peer can close its handle just before a normal process exit becomes observable.
constexpr auto ClosedConnectionGrace = std::chrono::milliseconds(250);
} // namespace

WorkerProcess::WorkerProcess(const ProcessCommand& command, WorkerProcessMode mode)
    : _mode(mode) {
    AwaitReady();
    const auto session = NewWorkerSession();
    _channel = std::make_unique<ControlChannel>(session, true);
    if (mode == WorkerProcessMode::Scene) _frames = std::make_unique<SharedFrames>(session, true);
    auto launch = command;
    launch.arguments.insert(launch.arguments.end(), {"--session", session});
    if (mode == WorkerProcessMode::Catalog) launch.arguments.push_back("--catalog");
    _process.Start(launch);
}
WorkerProcess::~WorkerProcess() { _process.Clear(); }
std::vector<WorkerMessage> WorkerProcess::Poll() { return _channel->Poll(); }
std::optional<ViewportPixels> WorkerProcess::ReadFrame(std::uint64_t generation) {
    return _frames ? _frames->Read(generation) : std::nullopt;
}
void WorkerProcess::Send(std::uint64_t generation, std::uint64_t request, const std::string& type,
                         nlohmann::json data) {
    if (_channel->Closed()) throw std::runtime_error("No scene connection");
    _channel->Send({{}, generation, request, type, std::move(data)});
}
bool WorkerProcess::ConnectionLost() {
    if (!_channel->Closed() || !Running() || ShuttingDown() || _mode == WorkerProcessMode::Catalog) return false;
    const auto now = std::chrono::steady_clock::now();
    if (!_closedAt) _closedAt = now;
    return now - *_closedAt > ClosedConnectionGrace;
}
void WorkerProcess::AwaitReady() { _startupDeadline = std::chrono::steady_clock::now() + StartupLimit; }
bool WorkerProcess::StartupTimedOut() const {
    return _startupDeadline && !ShuttingDown() && std::chrono::steady_clock::now() > *_startupDeadline;
}
void WorkerProcess::Shutdown() {
    if (ShuttingDown()) return;
    _shutdownDeadline = std::chrono::steady_clock::now() + ShutdownGrace;
    if (!Running()) return;
    if (_channel->Closed())
        _process.Terminate();
    else
        _channel->Send({{}, 0, 0, "shutdown", nlohmann::json::object()});
}
bool WorkerProcess::EnforceShutdown() {
    if (_terminated || !_shutdownDeadline || !Running() || std::chrono::steady_clock::now() <= *_shutdownDeadline)
        return false;
    _process.Terminate();
    _terminated = true;
    return true;
}
