#pragma once
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <vector>
#include "Modules/JsonIO/JsonIO.hpp"

class Game;

class Profiler {
public:
    using ScopeId = std::uint32_t;
    struct ScopeSample {
        ScopeId id = 0;
        double inclusiveMs = 0;
        double selfMs = 0;
    };
    struct CounterSample {
        ScopeId id = 0;
        double value = 0;
    };
    struct FrameSample {
        std::uint64_t id = 0;
        double intervalMs = 0;
        std::optional<double> gpuMs;
        std::vector<ScopeSample> scopes;
        std::vector<CounterSample> counters;
    };
    struct Recipe {
        int warmupFrames = 120;
        int measureFrames = 600;
        int pairs = 3;
        double stepSeconds = 1.0 / 60.0;
    };
    struct Metadata {
        std::string scenario, parameters, graphics, execution, environment, build, binary, gpu, cpu, machine;
        int scenarioVersion = 1;
        int width = 0, height = 0;
        std::uint32_t seed = 1729;
        Recipe recipe;
    };
    struct Run {
        int variant = 0;
        int repeat = 0;
        std::vector<FrameSample> frames;
    };
    struct Capture {
        Metadata metadata;
        std::vector<std::string> scopes, counters;
        std::vector<std::string> variants{"A", "B"};
        std::vector<Run> runs;
        bool completed = false;
    };
    struct Statistics {
        std::size_t count = 0;
        double mean = 0, median = 0, p95 = 0, p99 = 0, maximum = 0;
    };
    struct NamedStatistics {
        std::string name;
        Statistics inclusive, self;
    };
    struct Summary {
        Statistics frame, gpu;
        std::vector<NamedStatistics> scopes;
        std::vector<NamedStatistics> counters;
        double fps = 0;
        std::size_t missingGpu = 0;
    };
    struct Comparison {
        Summary baseline, current;
        bool compatible = false;
        double deltaMs = 0, improvementPercent = 0;
        std::string assessment;
        std::vector<std::string> reasons;
    };
    class Measurement {
    public:
        ~Measurement();

    private:
        friend class Profiler;
        explicit Measurement(Profiler* profiler);
        Profiler* _profiler;

    public:
        Measurement(const Measurement&) = delete;
        Measurement& operator=(const Measurement&) = delete;
    };

    explicit Profiler(std::size_t capacity = 4096);
    ~Profiler();
    void SetEnabled(bool enabled) {
        if (_enabled != enabled) _previousEnd.reset();
        _enabled = enabled;
    }
    [[nodiscard]] bool Enabled() const { return _enabled; }
    // Optional EngineProfilerRuntime target. Game/GL context must outlive this attachment.
    void Attach(Game& game);
    void Detach();
    void PollGpu();
    [[nodiscard]] bool GpuSupported() const;
    [[nodiscard]] std::size_t PendingGpu() const;
    [[nodiscard]] Metadata RuntimeMetadata() const;
    [[nodiscard]] ScopeId RegisterScope(const std::string& name);
    [[nodiscard]] ScopeId RegisterCounter(const std::string& name);
    [[nodiscard]] Measurement Measure(ScopeId id);
    void BeginFrame();
    void EndFrame();
    void RecordCounter(ScopeId id, double amount);
    // Importing measured samples is also useful to consumers with their own frame clocks.
    void RecordFrame(FrameSample frame);
    void SetGpuTime(std::uint64_t id, double milliseconds);
    void ClearFrames();
    [[nodiscard]] std::vector<FrameSample> TakeFrames();
    [[nodiscard]] const std::vector<std::string>& ScopeNames() const { return _scopes; }
    [[nodiscard]] const std::vector<std::string>& CounterNames() const { return _counters; }
    [[nodiscard]] std::uint64_t FrameId() const { return _nextFrame; }
    [[nodiscard]] std::size_t DroppedFrames() const { return _dropped; }
    [[nodiscard]] std::size_t BufferedFrames() const { return _frames.size(); }
    void BeginScope(ScopeId id);
    void EndScope();

    [[nodiscard]] static Statistics Describe(std::vector<double> values);
    [[nodiscard]] static Summary Summarize(const Capture& capture, int variant = 1);
    [[nodiscard]] static Comparison Compare(const Capture& baseline, const Capture& current, int baselineVariant = 1,
                                            int currentVariant = 1);
    [[nodiscard]] static std::vector<int> RunOrder(const Recipe& recipe);
    static void ValidateRecipe(const Recipe& recipe);
    [[nodiscard]] static JsonIO::Document Encode(const Capture& capture);
    [[nodiscard]] static Capture Decode(const JsonIO::Document& document);
    static void Save(const std::filesystem::path& file, const Capture& capture);
    [[nodiscard]] static Capture Load(const std::filesystem::path& file);
    [[nodiscard]] static std::string Markdown(const Capture& capture);

private:
    friend class ProfilerRuntimeBinding;
    struct Runtime;
    std::unique_ptr<Runtime> _runtime;
    using Clock = std::chrono::steady_clock;
    struct OpenScope {
        ScopeId id;
        Clock::time_point start;
        double children = 0;
    };
    void CompleteScope(Clock::time_point now);
    std::size_t _capacity, _dropped = 0;
    std::uint64_t _nextFrame = 0;
    std::vector<std::string> _scopes, _counters;
    std::vector<OpenScope> _stack;
    std::deque<FrameSample> _frames;
    FrameSample _frame;
    Clock::time_point _started;
    std::optional<Clock::time_point> _previousEnd;
    bool _recording = false;
    bool _enabled = false;
};
