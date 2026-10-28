#include "Modules/Profiler/Profiler.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <map>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace {
using J = JsonIO::Document;
constexpr std::size_t ReportLimit = 128 * 1024 * 1024;
J Strings(const std::vector<std::string>& strings) {
    J result = J::MakeArray();
    for (const auto& text : strings)
        result.Append(text);
    return result;
}
std::vector<std::string> ReadStrings(const J& value) {
    if (value.Elements().size() > 256) throw std::length_error("Profiler name limit exceeded");
    std::vector<std::string> result;
    std::set<std::string> seen;
    for (const auto& item : value.Elements()) {
        const auto& name = item.AsString();
        if (name.empty() || name.size() > 1024 || !seen.insert(name).second)
            throw std::invalid_argument("Invalid profiler name");
        result.push_back(name);
    }
    return result;
}
int ReadInt(const J& value, int minimum, int maximum) {
    const auto number = value.AsInt64();
    if (number < minimum || number > maximum) throw std::invalid_argument("Profiler integer out of range");
    return static_cast<int>(number);
}
Profiler::ScopeId ReadId(const J& value) {
    const auto number = value.AsUInt64();
    if (number >= 256) throw std::invalid_argument("Profiler ID out of range");
    return static_cast<Profiler::ScopeId>(number);
}
} // namespace

Profiler::Summary Profiler::Summarize(const Capture& capture, int variant) {
    Summary result;
    std::vector<double> intervals, gpu;
    std::vector<std::vector<double>> inclusive(capture.scopes.size()), self(capture.scopes.size()),
        counters(capture.counters.size());
    for (const Run& run : capture.runs) {
        if (run.variant != variant) continue;
        for (const FrameSample& frame : run.frames) {
            intervals.push_back(frame.intervalMs);
            if (frame.gpuMs)
                gpu.push_back(*frame.gpuMs);
            else
                ++result.missingGpu;
            for (std::size_t i = 0; i < inclusive.size(); ++i) {
                const auto found = std::ranges::find(frame.scopes, i, &ScopeSample::id);
                inclusive[i].push_back(found == frame.scopes.end() ? 0 : found->inclusiveMs);
                self[i].push_back(found == frame.scopes.end() ? 0 : found->selfMs);
            }
            for (std::size_t i = 0; i < counters.size(); ++i) {
                const auto found = std::ranges::find(frame.counters, i, &CounterSample::id);
                counters[i].push_back(found == frame.counters.end() ? 0 : found->value);
            }
        }
    }
    result.frame = Describe(std::move(intervals));
    result.gpu = Describe(std::move(gpu));
    result.fps = result.frame.mean > 0 ? 1000.0 / result.frame.mean : 0;
    for (std::size_t i = 0; i < inclusive.size(); ++i)
        result.scopes.push_back({capture.scopes[i], Describe(std::move(inclusive[i])), Describe(std::move(self[i]))});
    for (std::size_t i = 0; i < counters.size(); ++i)
        result.counters.push_back({capture.counters[i], Describe(std::move(counters[i])), {}});
    return result;
}
Profiler::Comparison Profiler::Compare(const Capture& baseline, const Capture& current, int a, int b) {
    Comparison result;
    result.baseline = Summarize(baseline, a);
    result.current = Summarize(current, b);
    const auto& x = baseline.metadata;
    const auto& y = current.metadata;
    const auto require = [&](bool condition, const char* reason) {
        if (!condition) result.reasons.emplace_back(reason);
    };
    require(baseline.completed && current.completed, "Capture is incomplete");
    const auto completeVariant = [](const Capture& capture, int variant) {
        if (variant < 0 || static_cast<std::size_t>(variant) >= capture.variants.size()) return false;
        const auto& recipe = capture.metadata.recipe;
        if (recipe.pairs < 1 || recipe.pairs > 6 || recipe.measureFrames < 2) return false;
        std::set<int> repeats;
        for (const auto& run : capture.runs) {
            if (run.variant != variant) continue;
            if (run.repeat < 0 || run.repeat >= recipe.pairs || !repeats.insert(run.repeat).second ||
                run.frames.size() != static_cast<std::size_t>(recipe.measureFrames))
                return false;
        }
        return repeats.size() == static_cast<std::size_t>(recipe.pairs);
    };
    require(completeVariant(baseline, a) && completeVariant(current, b), "Incomplete or repeated benchmark runs");
    require(x.scenario == y.scenario && x.scenarioVersion == y.scenarioVersion, "Scenario/version differs");
    require(x.parameters == y.parameters && x.seed == y.seed, "Parameters/seed differ");
    require(x.graphics == y.graphics && x.width == y.width && x.height == y.height, "Graphics/resolution differs");
    require(x.execution == y.execution, "Execution/variant policy differs");
    require(x.environment == y.environment && x.machine == y.machine && x.cpu == y.cpu && x.gpu == y.gpu,
            "Machine/GPU/driver environment differs");
    require(x.build == y.build, "Build configuration differs");
    require(x.recipe.stepSeconds == y.recipe.stepSeconds && x.recipe.warmupFrames == y.recipe.warmupFrames &&
                x.recipe.measureFrames == y.recipe.measureFrames && x.recipe.pairs == y.recipe.pairs,
            "Recipe differs");
    require(result.baseline.frame.count > 0 && result.current.frame.count > 0, "No measured frames");
    result.compatible = result.reasons.empty();
    result.deltaMs = result.current.frame.median - result.baseline.frame.median;
    if (result.baseline.frame.median > 0)
        result.improvementPercent = -result.deltaMs * 100.0 / result.baseline.frame.median;
    result.assessment = "Incompatible";
    if (!result.compatible) return result;
    std::map<int, Statistics> oldRuns, newRuns;
    const auto collect = [](const Capture& capture, int variant, auto& runs) {
        for (const Run& run : capture.runs) {
            if (run.variant != variant) continue;
            std::vector<double> values;
            for (const auto& frame : run.frames)
                values.push_back(frame.intervalMs);
            runs[run.repeat] = Describe(std::move(values));
        }
    };
    collect(baseline, a, oldRuns);
    collect(current, b, newRuns);
    bool faster = true, slower = true;
    for (int repeat = 0; repeat < x.recipe.pairs; ++repeat) {
        if (!oldRuns.contains(repeat) || !newRuns.contains(repeat) ||
            oldRuns[repeat].count != static_cast<std::size_t>(x.recipe.measureFrames) ||
            newRuns[repeat].count != static_cast<std::size_t>(y.recipe.measureFrames)) {
            faster = slower = false;
            break;
        }
        faster = faster && newRuns[repeat].median < oldRuns[repeat].median;
        slower = slower && newRuns[repeat].median > oldRuns[repeat].median;
    }
    result.assessment = "Uncertain";
    if (std::abs(result.improvementPercent) < 3.0 || x.recipe.pairs < 3) return result;
    if (faster)
        result.assessment = result.current.frame.p95 > result.baseline.frame.p95 * 1.03
                                ? "Faster median; P95 regression"
                                : "Measured improvement";
    else if (slower)
        result.assessment = "Measured regression";
    return result;
}
JsonIO::Document Profiler::Encode(const Capture& capture) {
    const auto& m = capture.metadata;
    J root = J::Object{{"version", 1},
                       {"completed", capture.completed},
                       {"scopes", Strings(capture.scopes)},
                       {"counters", Strings(capture.counters)},
                       {"variants", Strings(capture.variants)}};
    root["metadata"] = J::Object{{"scenario", m.scenario},
                                 {"scenarioVersion", m.scenarioVersion},
                                 {"parameters", m.parameters},
                                 {"graphics", m.graphics},
                                 {"execution", m.execution},
                                 {"environment", m.environment},
                                 {"build", m.build},
                                 {"binary", m.binary},
                                 {"gpu", m.gpu},
                                 {"cpu", m.cpu},
                                 {"machine", m.machine},
                                 {"seed", m.seed},
                                 {"width", m.width},
                                 {"height", m.height},
                                 {"warmup", m.recipe.warmupFrames},
                                 {"frames", m.recipe.measureFrames},
                                 {"pairs", m.recipe.pairs},
                                 {"step", m.recipe.stepSeconds}};
    root["runs"] = J::MakeArray();
    for (const Run& run : capture.runs) {
        J frames = J::MakeArray();
        for (const FrameSample& frame : run.frames) {
            J scopes = J::MakeArray(), counters = J::MakeArray();
            for (const auto& sample : frame.scopes)
                scopes.Append(J::Array{sample.id, sample.inclusiveMs, sample.selfMs});
            for (const auto& sample : frame.counters)
                counters.Append(J::Array{sample.id, sample.value});
            frames.Append(J::Object{{"id", frame.id},
                                    {"ms", frame.intervalMs},
                                    {"gpu", frame.gpuMs ? J(*frame.gpuMs) : J{}},
                                    {"scopes", std::move(scopes)},
                                    {"counters", std::move(counters)}});
        }
        root["runs"].Append(J::Object{{"variant", run.variant}, {"repeat", run.repeat}, {"frames", std::move(frames)}});
    }
    return root;
}
Profiler::Capture Profiler::Decode(const J& root) {
    if (root.At("version").AsInt64() != 1) throw std::invalid_argument("Unsupported profiler report version");
    Capture result;
    result.completed = root.At("completed").AsBool();
    result.scopes = ReadStrings(root.At("scopes"));
    result.counters = ReadStrings(root.At("counters"));
    result.variants = ReadStrings(root.At("variants"));
    const auto& m = root.At("metadata");
    auto& out = result.metadata;
    out.scenario = m.At("scenario").AsString();
    out.scenarioVersion = ReadInt(m.At("scenarioVersion"), 1, std::numeric_limits<int>::max());
    out.parameters = m.At("parameters").AsString();
    out.graphics = m.At("graphics").AsString();
    out.execution = m.At("execution").AsString();
    out.environment = m.At("environment").AsString();
    out.build = m.At("build").AsString();
    out.binary = m.At("binary").AsString();
    out.gpu = m.At("gpu").AsString();
    out.cpu = m.At("cpu").AsString();
    out.machine = m.At("machine").AsString();
    out.width = ReadInt(m.At("width"), 0, 32768);
    out.height = ReadInt(m.At("height"), 0, 32768);
    const auto seed = m.At("seed").AsUInt64();
    if (seed > std::numeric_limits<std::uint32_t>::max()) throw std::invalid_argument("Profiler seed out of range");
    out.seed = static_cast<std::uint32_t>(seed);
    out.recipe = {ReadInt(m.At("warmup"), 0, 10000), ReadInt(m.At("frames"), 2, 10000), ReadInt(m.At("pairs"), 1, 6),
                  m.At("step").AsDouble()};
    ValidateRecipe(out.recipe);
    if (root.At("runs").Elements().size() > 12) throw std::length_error("Profiler run limit exceeded");
    Profiler validator;
    for (const auto& name : result.scopes)
        (void)validator.RegisterScope(name);
    for (const auto& name : result.counters)
        (void)validator.RegisterCounter(name);
    for (const auto& run : root.At("runs").Elements()) {
        Run next;
        next.variant = ReadInt(run.At("variant"), 0, 255);
        next.repeat = ReadInt(run.At("repeat"), 0, 5);
        if (next.variant < 0 || static_cast<std::size_t>(next.variant) >= result.variants.size() || next.repeat < 0 ||
            next.repeat >= 6 || run.At("frames").Elements().size() > 10000)
            throw std::invalid_argument("Invalid profiler run");
        for (const auto& frame : run.At("frames").Elements()) {
            FrameSample sample;
            sample.id = frame.At("id").AsUInt64();
            sample.intervalMs = frame.At("ms").AsDouble();
            if (!frame.At("gpu").IsNull()) sample.gpuMs = frame.At("gpu").AsDouble();
            if (frame.At("scopes").Elements().size() > 256 || frame.At("counters").Elements().size() > 256)
                throw std::length_error("Profiler sample limit exceeded");
            for (const auto& value : frame.At("scopes").Elements()) {
                const auto& row = value.Elements();
                if (row.size() != 3) throw std::invalid_argument("Invalid profiler scope shape");
                sample.scopes.push_back({ReadId(row.at(0)), row.at(1).AsDouble(), row.at(2).AsDouble()});
            }
            for (const auto& value : frame.At("counters").Elements()) {
                const auto& row = value.Elements();
                if (row.size() != 2) throw std::invalid_argument("Invalid profiler counter shape");
                sample.counters.push_back({ReadId(row.at(0)), row.at(1).AsDouble()});
            }
            validator.RecordFrame(sample);
            next.frames.push_back(std::move(sample));
        }
        validator.ClearFrames();
        result.runs.push_back(std::move(next));
    }
    return result;
}
void Profiler::Save(const std::filesystem::path& file, const Capture& capture) {
    JsonIO::Write(file, Encode(capture), {.limits = {.maxBytes = ReportLimit}});
}
Profiler::Capture Profiler::Load(const std::filesystem::path& file) {
    return Decode(JsonIO::Read(file, {.limits = {.maxBytes = ReportLimit}}));
}
std::string Profiler::Markdown(const Capture& capture) {
    std::ostringstream text;
    text << "# Profiler: " << capture.metadata.scenario << "\n\nBuild: " << capture.metadata.build
         << "\n\nResolution: " << capture.metadata.width << "x" << capture.metadata.height
         << " | seed: " << capture.metadata.seed << " | warmup: " << capture.metadata.recipe.warmupFrames
         << " | measured frames/run: " << capture.metadata.recipe.measureFrames
         << " | pairs: " << capture.metadata.recipe.pairs << "\n\nGPU: " << capture.metadata.gpu
         << "\n\nBinary: " << capture.metadata.binary
         << "\n\n| Variant | FPS | Median ms | P95 ms | P99 ms | GPU mean ms | GPU samples |\n"
         << "|---|---:|---:|---:|---:|---:|---:|\n"
         << std::fixed << std::setprecision(3);
    for (std::size_t i = 0; i < capture.variants.size(); ++i) {
        const auto s = Summarize(capture, static_cast<int>(i));
        text << "| " << capture.variants[i] << " | " << s.fps << " | " << s.frame.median << " | " << s.frame.p95
             << " | " << s.frame.p99 << " | ";
        if (s.gpu.count)
            text << s.gpu.mean;
        else
            text << "Unavailable";
        text << " | " << s.gpu.count << "/" << s.frame.count << " |\n";
    }
    if (capture.variants.size() >= 2) {
        const auto comparison = Compare(capture, capture, 0, 1);
        text << "\n" << comparison.assessment << ": " << comparison.improvementPercent << "% frame median change.\n";
    }
    for (std::size_t i = 0; i < capture.variants.size(); ++i) {
        const auto summary = Summarize(capture, static_cast<int>(i));
        text << "\n## " << capture.variants[i]
             << "\n\n| CPU scope | Mean ms | Self mean ms | P95 ms | P99 ms | Max ms |\n"
             << "|---|---:|---:|---:|---:|---:|\n";
        for (const auto& scope : summary.scopes)
            text << "| " << scope.name << " | " << scope.inclusive.mean << " | " << scope.self.mean << " | "
                 << scope.inclusive.p95 << " | " << scope.inclusive.p99 << " | " << scope.inclusive.maximum << " |\n";
        text << "\n| Counter | Mean/frame |\n|---|---:|\n";
        for (const auto& counter : summary.counters)
            text << "| " << counter.name << " | " << counter.inclusive.mean << " |\n";
    }
    text << "\n## Repeats\n\n| Variant | Repeat | FPS | Median ms | P95 ms |\n|---|---:|---:|---:|---:|\n";
    for (const auto& run : capture.runs) {
        Capture part;
        part.runs = {run};
        const auto summary = Summarize(part, run.variant);
        text << "| " << capture.variants[static_cast<std::size_t>(run.variant)] << " | " << run.repeat + 1 << " | "
             << summary.fps << " | " << summary.frame.median << " | " << summary.frame.p95 << " |\n";
    }
    text << "\nCPU and GPU times overlap. Glow blend requests are not total GL draw calls.\n";
    return text.str();
}
