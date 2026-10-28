#pragma once
#include <optional>
#include "Modules/Profiler/Profiler.hpp"
#include "Runtime/FrameExecutionSettings.h"

enum class BenchmarkPhase { Idle, Preparing, Warmup, Measuring, CollectingGpu, Completed, Cancelled };

// Pure benchmark scheduling; the worker owns scene resets, GL queries and setting restoration.
class BenchmarkState {
public:
    void Start(const Profiler::Recipe& recipe, FrameExecutionSettings execution = {});
    void BeginRun();
    bool AddWarmup(std::size_t completed);
    bool Measured(std::size_t completed);
    bool FinishRun();
    void Cancel();
    [[nodiscard]] std::optional<FrameExecutionSettings> TakeSavedExecution();
    [[nodiscard]] BenchmarkPhase Phase() const { return _phase; }
    [[nodiscard]] std::size_t RunIndex() const { return _run; }
    [[nodiscard]] std::size_t WarmupCount() const { return _warmup; }
    [[nodiscard]] int Variant() const;

private:
    Profiler::Recipe _recipe;
    BenchmarkPhase _phase = BenchmarkPhase::Idle;
    std::vector<int> _order;
    std::size_t _run = 0, _warmup = 0;
    std::optional<FrameExecutionSettings> _execution;
};
