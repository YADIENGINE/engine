#include "Editor/Application/PreviewWorker.h"
#include <stdexcept>
#include <utility>
#include "Editor/Internals/Platform/Windows/WindowsText.h"

namespace {
constexpr auto StartupLimit = std::chrono::seconds(30); // Until the first frame (Preview) or the catalog exit
constexpr auto ShutdownGrace = std::chrono::seconds(5);
// The peer can close its handle just before a normal process exit becomes observable.
constexpr auto ClosedConnectionGrace = std::chrono::milliseconds(250);
} // namespace

PreviewWorker::PreviewWorker(const ProcessCommand& command, PreviewWorkerMode mode)
    : _mode(mode) {
    AwaitReady();
    const auto session = NewPreviewSession();
    _channel = std::make_unique<ControlChannel>(session, true);
    if (mode == PreviewWorkerMode::Preview) _frames = std::make_unique<SharedFrames>(session, true);
    auto launch = command;
    launch.arguments.insert(launch.arguments.end(), {"--session", session});
    if (mode == PreviewWorkerMode::Catalog) launch.arguments.push_back("--catalog");
    _process.Start(launch);
}
PreviewWorker::~PreviewWorker() { _process.Clear(); }
std::vector<PreviewMessage> PreviewWorker::Poll() { return _channel->Poll(); }
std::optional<PreviewPixels> PreviewWorker::ReadFrame(std::uint64_t generation) {
    return _frames ? _frames->Read(generation) : std::nullopt;
}
void PreviewWorker::Send(std::uint64_t generation, std::uint64_t request, const std::string& type,
                         nlohmann::json data) {
    if (_channel->Closed()) throw std::runtime_error("No preview connection");
    _channel->Send({{}, generation, request, type, std::move(data)});
}
bool PreviewWorker::ConnectionLost() {
    if (!_channel->Closed() || !Running() || ShuttingDown() || _mode == PreviewWorkerMode::Catalog) return false;
    const auto now = std::chrono::steady_clock::now();
    if (!_closedAt) _closedAt = now;
    return now - *_closedAt > ClosedConnectionGrace;
}
void PreviewWorker::AwaitReady() { _startupDeadline = std::chrono::steady_clock::now() + StartupLimit; }
bool PreviewWorker::StartupTimedOut() const {
    return _startupDeadline && !ShuttingDown() && std::chrono::steady_clock::now() > *_startupDeadline;
}
void PreviewWorker::Shutdown() {
    if (ShuttingDown()) return;
    _shutdownDeadline = std::chrono::steady_clock::now() + ShutdownGrace;
    if (!Running()) return;
    if (_channel->Closed())
        _process.Terminate();
    else
        _channel->Send({{}, 0, 0, "shutdown", nlohmann::json::object()});
}
bool PreviewWorker::EnforceShutdown() {
    if (_terminated || !_shutdownDeadline || !Running() || std::chrono::steady_clock::now() <= *_shutdownDeadline)
        return false;
    _process.Terminate();
    _terminated = true;
    return true;
}
