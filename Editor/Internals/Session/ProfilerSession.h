#pragma once
#include <memory>
#include "Modules/Profiler/Profiler.hpp"
#include "Editor/Internals/Protocol/PreviewProtocol.h"

struct ProfilerSessionSnapshot {
    bool liveEnabled = false, busy = false, gpuSupported = false;
    std::string phase = "Idle", error, file;
    std::size_t run = 0, runs = 0, frame = 0, target = 0;
    std::uint64_t captureId = 0;
    std::shared_ptr<const Profiler::Capture> live, result;
    std::shared_ptr<const Profiler::Summary> liveSummary;
    std::shared_ptr<const Profiler::Comparison> variants, baseline;
};

// Bounded, headless assembler. No UI, processes or file IO; a capture becomes visible only on complete.
class ProfilerSession {
public:
    void Reset(std::uint64_t generation);
    void Arm(std::uint64_t captureId);
    void Disconnected();
    void Receive(const PreviewMessage& message);
    void SetBaseline(Profiler::Capture capture);
    void SetFile(const std::string& file) { _snapshot.file = file; }
    void Error(const std::string& error) { _snapshot.error = error; }
    [[nodiscard]] const ProfilerSessionSnapshot& Snapshot() const { return _snapshot; }

private:
    void Finish(const std::string& kind);
    ProfilerSessionSnapshot _snapshot;
    std::uint64_t _generation = 0;
    std::optional<Profiler::Capture> _incoming, _baseline;
    std::string _kind;
    std::size_t _expectedRuns = 0;
};
