#include "Editor/Internals/Session/ProfilerSession.h"
#include <algorithm>
#include <stdexcept>

void ProfilerSession::Reset(std::uint64_t generation) {
    const bool interrupted = _snapshot.busy;
    _generation = generation;
    _incoming.reset();
    _snapshot.busy = false;
    if (_snapshot.phase != "Failed") _snapshot.phase = "Idle";
    _snapshot.captureId = 0;
    _snapshot.live.reset();
    _snapshot.liveSummary.reset();
    if (_snapshot.phase != "Failed") _snapshot.error.clear();
    if (interrupted) {
        _snapshot.phase = "Failed";
        _snapshot.error = "Capture interrupted by session reset";
    }
}
void ProfilerSession::Arm(std::uint64_t captureId) {
    if (!captureId) throw std::invalid_argument("Invalid profiler capture ID");
    _snapshot.captureId = captureId;
    _snapshot.busy = true;
    _snapshot.phase = "Preparing";
    _snapshot.error.clear();
    _incoming.reset();
}
void ProfilerSession::Disconnected() {
    if (_snapshot.busy) {
        _snapshot.phase = "Failed";
        _snapshot.error = "Worker disconnected; capture is incomplete";
    }
    _snapshot.busy = false;
    _snapshot.liveEnabled = false;
    _incoming.reset();
}
void ProfilerSession::Receive(const WorkerMessage& message) {
    if (message.generation != _generation) return;
    const auto id = message.data.at("captureId").get<std::uint64_t>();
    if (id != _snapshot.captureId) return;
    const auto& data = message.data;
    if (message.type == "profiler-state") {
        _snapshot.phase = data.at("phase").get<std::string>();
        _snapshot.busy = data.at("busy").get<bool>();
        _snapshot.liveEnabled = data.at("live").get<bool>();
        _snapshot.gpuSupported = data.at("gpuSupported").get<bool>();
        _snapshot.run = data.at("run").get<std::size_t>();
        _snapshot.runs = data.at("runs").get<std::size_t>();
        _snapshot.frame = data.at("frame").get<std::size_t>();
        _snapshot.target = data.at("target").get<std::size_t>();
        _snapshot.error = data.value("error", std::string{});
        if (!_snapshot.busy && (_snapshot.phase == "Failed" || _snapshot.phase == "Cancelled")) _incoming.reset();
        return;
    }
    const auto kind = data.at("kind").get<std::string>();
    if (kind != "live" && kind != "benchmark") throw std::runtime_error("Invalid profiler capture kind");
    if (message.type == "profiler-result") {
        _expectedRuns = data.at("runCount").get<std::size_t>();
        if (_expectedRuns > 12 || !_expectedRuns) throw std::runtime_error("Invalid profiler run count");
        _incoming = Profiler::Decode(JsonIO::Parse(data.at("header").dump()));
        _kind = kind;
        if (!_incoming->runs.empty()) throw std::runtime_error("Profiler header contains samples");
    } else if (message.type == "profiler-chunk") {
        if (!_incoming || kind != _kind) return;
        const auto index = data.at("run").get<std::size_t>();
        const auto offset = data.at("offset").get<std::size_t>();
        const auto part = Profiler::Decode(JsonIO::Parse(data.at("capture").dump()));
        if (index >= _expectedRuns || part.runs.size() != 1 || part.runs[0].frames.empty() ||
            part.runs[0].frames.size() > 16 || part.scopes != _incoming->scopes ||
            part.counters != _incoming->counters || part.variants != _incoming->variants ||
            part.metadata.scenario != _incoming->metadata.scenario)
            throw std::runtime_error("Invalid profiler chunk");
        if (index == _incoming->runs.size()) _incoming->runs.push_back({part.runs[0].variant, part.runs[0].repeat, {}});
        if (index >= _incoming->runs.size()) throw std::runtime_error("Out-of-order profiler run");
        auto& run = _incoming->runs[index];
        if (offset != run.frames.size() || run.variant != part.runs[0].variant || run.repeat != part.runs[0].repeat ||
            offset + part.runs[0].frames.size() > 10000)
            throw std::runtime_error("Out-of-order profiler frames");
        run.frames.insert(run.frames.end(), part.runs[0].frames.begin(), part.runs[0].frames.end());
    } else if (message.type == "profiler-complete") {
        if (_incoming && kind == _kind) Finish(kind);
    }
}
void ProfilerSession::Finish(const std::string& kind) {
    if (_incoming->runs.size() != _expectedRuns) throw std::runtime_error("Incomplete profiler transfer");
    if (kind == "benchmark") {
        const auto& recipe = _incoming->metadata.recipe;
        const auto order = Profiler::RunOrder(recipe);
        if (!_incoming->completed || order.size() != _incoming->runs.size())
            throw std::runtime_error("Incomplete benchmark");
        for (std::size_t i = 0; i < order.size(); ++i)
            if (_incoming->runs[i].variant != order[i] || _incoming->runs[i].repeat != static_cast<int>(i / 2) ||
                _incoming->runs[i].frames.size() != static_cast<std::size_t>(recipe.measureFrames))
                throw std::runtime_error("Incomplete benchmark run");
        _snapshot.result = std::make_shared<Profiler::Capture>(std::move(*_incoming));
        _snapshot.variants =
            std::make_shared<Profiler::Comparison>(Profiler::Compare(*_snapshot.result, *_snapshot.result, 0, 1));
        if (_baseline)
            _snapshot.baseline =
                std::make_shared<Profiler::Comparison>(Profiler::Compare(*_baseline, *_snapshot.result));
    } else {
        if (_incoming->runs.size() != 1 || _incoming->runs[0].frames.size() > 120)
            throw std::runtime_error("Invalid live profiler batch");
        Profiler::Capture live = std::move(*_incoming);
        if (_snapshot.live && _snapshot.live->metadata.scenario == live.metadata.scenario &&
            _snapshot.live->scopes == live.scopes && _snapshot.live->counters == live.counters) {
            auto frames = _snapshot.live->runs[0].frames;
            frames.insert(frames.end(), live.runs[0].frames.begin(), live.runs[0].frames.end());
            if (frames.size() > 600) frames.erase(frames.begin(), frames.end() - 600);
            live.runs[0].frames = std::move(frames);
        }
        _snapshot.live = std::make_shared<Profiler::Capture>(std::move(live));
        _snapshot.liveSummary = std::make_shared<Profiler::Summary>(Profiler::Summarize(*_snapshot.live));
    }
    _incoming.reset();
}
void ProfilerSession::SetBaseline(Profiler::Capture capture) {
    _baseline = std::move(capture);
    if (_snapshot.result)
        _snapshot.baseline = std::make_shared<Profiler::Comparison>(Profiler::Compare(*_baseline, *_snapshot.result));
}
