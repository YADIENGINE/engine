#include "Editor/Internals/Session/BenchmarkState.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

void BenchmarkState::Start(const Profiler::Recipe& recipe, FrameExecutionSettings execution) {
    _order = Profiler::RunOrder(recipe);
    _recipe = recipe;
    _run = _warmup = 0;
    _phase = BenchmarkPhase::Preparing;
    _execution = execution;
}
void BenchmarkState::BeginRun() {
    if (_phase != BenchmarkPhase::Preparing) throw std::logic_error("Benchmark is not preparing a run");
    _warmup = 0;
    _phase = _recipe.warmupFrames ? BenchmarkPhase::Warmup : BenchmarkPhase::Measuring;
}
bool BenchmarkState::AddWarmup(std::size_t completed) {
    if (_phase != BenchmarkPhase::Warmup) return false;
    _warmup = std::min(_warmup + completed, static_cast<std::size_t>(_recipe.warmupFrames));
    if (_warmup < static_cast<std::size_t>(_recipe.warmupFrames)) return false;
    _phase = BenchmarkPhase::Measuring;
    return true;
}
bool BenchmarkState::Measured(std::size_t completed) {
    if (_phase != BenchmarkPhase::Measuring || completed < static_cast<std::size_t>(_recipe.measureFrames))
        return false;
    _phase = BenchmarkPhase::CollectingGpu;
    return true;
}
bool BenchmarkState::FinishRun() {
    if (_phase != BenchmarkPhase::CollectingGpu) throw std::logic_error("Benchmark run is not collecting results");
    ++_run;
    _phase = _run < _order.size() ? BenchmarkPhase::Preparing : BenchmarkPhase::Completed;
    return _phase == BenchmarkPhase::Preparing;
}
void BenchmarkState::Cancel() { _phase = BenchmarkPhase::Cancelled; }
std::optional<FrameExecutionSettings> BenchmarkState::TakeSavedExecution() {
    return std::exchange(_execution, std::nullopt);
}
int BenchmarkState::Variant() const { return _order.at(_run); }
