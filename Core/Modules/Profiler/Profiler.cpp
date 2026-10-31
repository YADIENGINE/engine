#include "Modules/Profiler/Profiler.h"
#include "Modules/Profiler/Internals/RuntimeState.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace {
std::uint32_t Register(std::vector<std::string>& names, const std::string& name) {
    if (name.empty() || name.size() > 1024) throw std::invalid_argument("Invalid profiler name");
    const auto found = std::ranges::find(names, name);
    if (found != names.end()) return static_cast<std::uint32_t>(found - names.begin());
    if (names.size() >= 256) throw std::length_error("Profiler name capacity exceeded");
    names.push_back(name);
    return static_cast<std::uint32_t>(names.size() - 1);
}
} // namespace

Profiler::Profiler(std::size_t capacity)
    : _capacity(capacity) {
    if (!capacity || capacity > 120000) throw std::invalid_argument("Invalid profiler capacity");
    _stack.reserve(64);
}
Profiler::~Profiler() = default;
Profiler::ScopeId Profiler::RegisterScope(const std::string& name) { return Register(_scopes, name); }
Profiler::ScopeId Profiler::RegisterCounter(const std::string& name) { return Register(_counters, name); }
Profiler::Measurement::Measurement(Profiler* profiler)
    : _profiler(profiler) {}
Profiler::Measurement::~Measurement() {
    if (_profiler) _profiler->EndScope();
}
Profiler::Measurement Profiler::Measure(ScopeId id) {
    if (!_recording) return Measurement(nullptr);
    BeginScope(id);
    return Measurement(this);
}
void Profiler::BeginFrame() {
    if (_recording) throw std::logic_error("Profiler frame already open");
    _started = Clock::now();
    _frame = {};
    _frame.id = ++_nextFrame;
    _frame.scopes.reserve(_scopes.size());
    _frame.counters.reserve(_counters.size());
    _stack.clear();
    _recording = true;
}
void Profiler::BeginScope(ScopeId id) {
    if (!_recording) return;
    if (id >= _scopes.size() || _stack.size() >= 64) throw std::out_of_range("Invalid profiler scope");
    _stack.push_back({id, Clock::now()});
}
void Profiler::CompleteScope(Clock::time_point now) {
    if (_stack.empty()) return;
    const OpenScope scope = _stack.back();
    _stack.pop_back();
    const double elapsed = std::chrono::duration<double, std::milli>(now - scope.start).count();
    auto found = std::ranges::find(_frame.scopes, scope.id, &ScopeSample::id);
    if (found == _frame.scopes.end()) {
        _frame.scopes.push_back({scope.id});
        found = std::prev(_frame.scopes.end());
    }
    found->inclusiveMs += elapsed;
    found->selfMs += std::max(0.0, elapsed - scope.children);
    if (!_stack.empty()) _stack.back().children += elapsed;
}
void Profiler::EndScope() {
    if (_recording) CompleteScope(Clock::now());
}
void Profiler::EndFrame() {
    if (!_recording) return;
    const auto now = Clock::now();
    while (!_stack.empty())
        CompleteScope(now);
    _frame.intervalMs = std::chrono::duration<double, std::milli>(now - _previousEnd.value_or(_started)).count();
    _previousEnd = now;
    _recording = false;
    RecordFrame(std::move(_frame));
}
void Profiler::RecordCounter(ScopeId id, double amount) {
    if (!_recording) return;
    if (id >= _counters.size() || !std::isfinite(amount) || amount < 0)
        throw std::invalid_argument("Invalid profiler counter");
    const auto found = std::ranges::find(_frame.counters, id, &CounterSample::id);
    if (found == _frame.counters.end())
        _frame.counters.push_back({id, amount});
    else
        found->value += amount;
}
void Profiler::RecordFrame(FrameSample frame) {
    const auto valid = [](double value) { return std::isfinite(value) && value >= 0; };
    if (!valid(frame.intervalMs) || (frame.gpuMs && !valid(*frame.gpuMs)))
        throw std::invalid_argument("Invalid profiler frame duration");
    std::array<bool, 256> scopes{}, counters{};
    for (const auto& scope : frame.scopes) {
        if (scope.id >= _scopes.size() || !valid(scope.inclusiveMs) || !valid(scope.selfMs) ||
            scope.selfMs > scope.inclusiveMs || scopes[scope.id])
            throw std::invalid_argument("Invalid profiler scope sample");
        scopes[scope.id] = true;
    }
    for (const auto& counter : frame.counters) {
        if (counter.id >= _counters.size() || !std::isfinite(counter.value) || counter.value < 0 ||
            counters[counter.id])
            throw std::invalid_argument("Invalid profiler counter sample");
        counters[counter.id] = true;
    }
    if (_frames.size() == _capacity) {
        _frames.pop_front();
        ++_dropped;
    }
    _frames.push_back(std::move(frame));
}
void Profiler::SetGpuTime(std::uint64_t id, double milliseconds) {
    if (!std::isfinite(milliseconds) || milliseconds < 0) return;
    for (auto frame = _frames.rbegin(); frame != _frames.rend(); ++frame)
        if (frame->id == id) {
            frame->gpuMs = milliseconds;
            return;
        }
}
void Profiler::ClearFrames() {
    _frames.clear();
    _dropped = 0;
    _previousEnd.reset();
}
std::vector<Profiler::FrameSample> Profiler::TakeFrames() {
    std::vector<FrameSample> result;
    result.reserve(_frames.size());
    for (auto& frame : _frames)
        result.push_back(std::move(frame));
    _frames.clear();
    return result;
}
Profiler::Statistics Profiler::Describe(std::vector<double> values) {
    if (values.empty()) return {};
    for (double value : values)
        if (!std::isfinite(value) || value < 0) throw std::invalid_argument("Invalid profiler statistic");
    std::ranges::sort(values);
    const auto quantile = [&](double p) {
        const double at = p * static_cast<double>(values.size() - 1);
        const auto low = static_cast<std::size_t>(at);
        const auto high = std::min(low + 1, values.size() - 1);
        return values[low] + (values[high] - values[low]) * (at - static_cast<double>(low));
    };
    double sum = 0;
    for (double value : values)
        sum += value;
    return {values.size(), sum / static_cast<double>(values.size()), quantile(0.5), quantile(0.95), quantile(0.99),
            values.back()};
}
void Profiler::ValidateRecipe(const Recipe& recipe) {
    if (recipe.warmupFrames < 0 || recipe.warmupFrames > 10000 || recipe.measureFrames < 2 ||
        recipe.measureFrames > 10000 || recipe.pairs < 1 || recipe.pairs > 6 || !std::isfinite(recipe.stepSeconds) ||
        recipe.stepSeconds <= 0 || recipe.stepSeconds > 1)
        throw std::invalid_argument("Invalid benchmark recipe");
}
std::vector<int> Profiler::RunOrder(const Recipe& recipe) {
    ValidateRecipe(recipe);
    std::vector<int> result;
    for (int pair = 0; pair < recipe.pairs; ++pair) {
        result.push_back(pair % 2);
        result.push_back(1 - pair % 2);
    }
    return result;
}
