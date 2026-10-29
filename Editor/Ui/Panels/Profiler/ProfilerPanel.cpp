#include "Editor/Ui/Panels/Profiler/ProfilerPanel.h"
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <string>
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

namespace {
void StatisticsRow(const char* name, const Profiler::Statistics& stats) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    ImGui::TextUnformatted(name);
    ImGui::PushFont(EditorFont(EditorFontRole::Technical), 0);
    for (const double value : {stats.mean, stats.median, stats.p95, stats.p99, stats.maximum}) {
        ImGui::TableNextColumn();
        if (stats.count)
            ImGui::Text("%.3f", value);
        else
            ImGui::TextDisabled("Unavailable");
    }
    ImGui::PopFont();
}
void Summary(const Profiler::Summary& summary) {
    ImGui::Text("Worker FPS %.1f | samples %zu | GPU samples %zu / %zu", summary.fps, summary.frame.count,
                summary.gpu.count, summary.frame.count);
    if (ImGui::BeginTable("durations", 6,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX)) {
        for (const auto* label : {"Metric (ms)", "Mean", "Median", "P95", "P99", "Max"})
            ImGui::TableSetupColumn(label);
        ImGui::TableHeadersRow();
        StatisticsRow("Frame interval", summary.frame);
        StatisticsRow("Canvas GPU", summary.gpu);
        for (const auto& scope : summary.scopes)
            StatisticsRow(scope.name.c_str(), scope.inclusive);
        ImGui::EndTable();
    }
    for (const auto& counter : summary.counters)
        ImGui::Text("%s: %.1f / frame", counter.name.c_str(), counter.inclusive.mean);
}
void Comparison(const Profiler::Comparison& comparison) {
    ImGui::TextWrapped("%s", comparison.assessment.c_str());
    if (comparison.compatible)
        ImGui::Text("Frame median: %+.3f ms | %.2f%% improvement", comparison.deltaMs, comparison.improvementPercent);
    else
        for (const auto& reason : comparison.reasons)
            ImGui::BulletText("%s", reason.c_str());
    if (ImGui::BeginTable("comparison", 5,
                          ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Metric");
        ImGui::TableSetupColumn("A / baseline");
        ImGui::TableSetupColumn("B / current");
        ImGui::TableSetupColumn("Delta B-A");
        ImGui::TableSetupColumn("Change %");
        ImGui::TableHeadersRow();
        const auto row = [](const char* label, double a, double b) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(label);
            ImGui::PushFont(EditorFont(EditorFontRole::Technical), 0);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", a);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", b);
            ImGui::TableNextColumn();
            ImGui::Text("%+.3f", b - a);
            ImGui::TableNextColumn();
            if (a != 0)
                ImGui::Text("%+.2f%%", (b - a) * 100.0 / a);
            else
                ImGui::TextUnformatted("--");
            ImGui::PopFont();
        };
        row("Worker FPS", comparison.baseline.fps, comparison.current.fps);
        row("Frame median ms", comparison.baseline.frame.median, comparison.current.frame.median);
        row("Frame P95 ms", comparison.baseline.frame.p95, comparison.current.frame.p95);
        row("Frame P99 ms", comparison.baseline.frame.p99, comparison.current.frame.p99);
        if (comparison.baseline.gpu.count && comparison.current.gpu.count)
            row("GPU mean ms", comparison.baseline.gpu.mean, comparison.current.gpu.mean);
        for (const auto& a : comparison.baseline.scopes) {
            const auto b = std::ranges::find(comparison.current.scopes, a.name, &Profiler::NamedStatistics::name);
            if (b != comparison.current.scopes.end()) row(a.name.c_str(), a.inclusive.mean, b->inclusive.mean);
        }
        for (const auto& a : comparison.baseline.counters) {
            const auto b = std::ranges::find(comparison.current.counters, a.name, &Profiler::NamedStatistics::name);
            if (b != comparison.current.counters.end()) row(a.name.c_str(), a.inclusive.mean, b->inclusive.mean);
        }
        ImGui::EndTable();
    }
    ImGui::TextWrapped("CPU and GPU overlap. Glow blend requests are helper requests, not total GL draw calls. Labels "
                       "are conservative observations, not statistical significance.");
}
} // namespace
void ProfilerPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    if (!actions.profiler) return;
    if (_glowGeneration != snapshot.generation) {
        _glowGeneration = snapshot.generation;
        _glow = -1;
    }
    const auto& profiler = snapshot.profiler;
    const auto phase = !profiler.error.empty() ? EditorPhase::Error
                       : profiler.busy         ? EditorPhase::Building
                       : snapshot.ready        ? EditorPhase::Ready
                                               : EditorPhase::Stopped;
    EditorStatusBadge(profiler.phase.c_str(), EditorPhaseColor(phase));
    ContinueEditorToolbar("GPU timer supported");
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", profiler.gpuSupported ? "GPU timer supported" : "GPU unavailable / inactive");
    if (!profiler.error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, EditorLevelColor(EditorLogLevel::Error));
        ImGui::TextWrapped("%s", profiler.error.c_str());
        ImGui::PopStyleColor();
    }
    if (ImGui::BeginTabBar("profiler-tabs")) {
        if (ImGui::BeginTabItem("Live")) {
            Live(snapshot, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Benchmarks")) {
            Benchmarks(snapshot, actions);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Compare")) {
            Compare(snapshot, actions);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}
void ProfilerPanel::Live(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    bool enabled = snapshot.profiler.liveEnabled;
    ImGui::BeginDisabled(!snapshot.ready || snapshot.profiler.busy);
    if (ImGui::Checkbox("Record live", &enabled))
        actions.profiler(enabled ? EditorProfilerCommand::Enable : EditorProfilerCommand::Disable, {}, {});
    ImGui::EndDisabled();
    ImGui::TextWrapped("Stream FPS %.1f | readback %.2f | publish %.2f | host upload %.2f ms", snapshot.streamFps,
                       snapshot.captureMs, snapshot.publishMs, snapshot.hostTransferMs);
    ImGui::TextWrapped("Live timing includes preview transport and the worker frame limit. Use Benchmarks for "
                       "controlled performance comparisons.");
    if (snapshot.profiler.liveSummary)
        Summary(*snapshot.profiler.liveSummary);
    else
        EditorEmptyState("No live samples", "Enable Record live while a preview is running to inspect timing.");
    if (snapshot.profiler.live) Frames(*snapshot.profiler.live);
}
void ProfilerPanel::Benchmarks(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    ImGui::BeginDisabled(snapshot.busy);
    const char* selected =
        snapshot.selected && snapshot.selected->benchmark ? snapshot.selected->name.c_str() : "Choose benchmark";
    if (ImGui::BeginCombo("Scenario", selected)) {
        for (const auto& entry : snapshot.catalog) {
            if (!entry.benchmark) continue;
            if (ImGui::Selectable(entry.name.c_str(), snapshot.selected && entry.id == snapshot.selected->id)) {
                actions.select(entry.id);
                actions.command(EditorCommand::Open);
            }
        }
        ImGui::EndCombo();
    }
    if (EditorSectionHeader("Advanced benchmark settings", false)) {
        ImGui::InputInt("Warmup frames", &_recipe.warmupFrames);
        ImGui::InputInt("Measured frames", &_recipe.measureFrames);
        ImGui::InputInt("Pairs", &_recipe.pairs);
    }
    ImGui::BeginDisabled(!snapshot.ready);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Glow override");
    EditorTooltip("Apply a preview glow quality override. The highlight shows the last override requested here.");
    constexpr std::array GlowLabels{"Full", "Reduced", "Off"};
    constexpr std::array GlowCommands{EditorProfilerCommand::GlowFull, EditorProfilerCommand::GlowReduced,
                                      EditorProfilerCommand::GlowOff};
    for (int i = 0; i < static_cast<int>(GlowLabels.size()); ++i) {
        ContinueEditorToolbar(GlowLabels[static_cast<std::size_t>(i)]);
        if (EditorSegment(GlowLabels[static_cast<std::size_t>(i)], _glow == i)) {
            _glow = i;
            actions.profiler(GlowCommands[static_cast<std::size_t>(i)], {}, {});
        }
    }
    ImGui::EndDisabled();
    _recipe.warmupFrames = std::clamp(_recipe.warmupFrames, 0, 10000);
    _recipe.measureFrames = std::clamp(_recipe.measureFrames, 2, 10000);
    _recipe.pairs = std::clamp(_recipe.pairs, 1, 6);
    ImGui::TextWrapped("Fixed 1/60 s simulation. A/B, B/A, A/B. Input and image transfer stop during measurement. The "
                       "preview resets afterwards. Release builds are recommended; Debug results stay labeled Debug.");
    ImGui::BeginDisabled(!snapshot.ready || !snapshot.selected || !snapshot.selected->benchmark ||
                         !snapshot.pending.empty());
    if (ImGui::Button("Compare Individual / Batched")) actions.profiler(EditorProfilerCommand::Start, _recipe, {});
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (snapshot.profiler.busy) {
        ImGui::Text("Run %zu / %zu | frame %zu / %zu", snapshot.profiler.run + 1, snapshot.profiler.runs,
                    snapshot.profiler.frame, snapshot.profiler.target);
        EditorCommandButton(EditorCommand::Cancel, snapshot, actions, true);
    }
    if (snapshot.profiler.variants) Comparison(*snapshot.profiler.variants);
    if (snapshot.profiler.result) {
        ImGui::TextWrapped("Completed capture: %s (%s)", snapshot.profiler.result->metadata.scenario.c_str(),
                           snapshot.profiler.result->metadata.build.c_str());
        if (_statisticsFor != snapshot.profiler.result) {
            _statisticsFor = snapshot.profiler.result;
            _runStatistics.clear();
            Profiler::Capture part;
            part.scopes = _statisticsFor->scopes;
            part.counters = _statisticsFor->counters;
            for (const auto& run : _statisticsFor->runs) {
                part.runs = {run};
                _runStatistics.push_back(Profiler::Summarize(part, run.variant));
            }
            for (int variant = 0; variant < 2; ++variant)
                _variantStatistics[static_cast<std::size_t>(variant)] = Profiler::Summarize(*_statisticsFor, variant);
        }
        for (std::size_t variant = 0; variant < _variantStatistics.size(); ++variant) {
            double minimum = DBL_MAX, maximum = 0;
            for (std::size_t i = 0; i < _runStatistics.size(); ++i)
                if (_statisticsFor->runs[i].variant == static_cast<int>(variant)) {
                    minimum = std::min(minimum, _runStatistics[i].frame.median);
                    maximum = std::max(maximum, _runStatistics[i].frame.median);
                }
            ImGui::Text("%s repeat medians: %.3f .. %.3f ms (%.1f%% span)", _statisticsFor->variants[variant].c_str(),
                        minimum, maximum, minimum > 0 ? (maximum - minimum) * 100.0 / minimum : 0);
            const std::string label = "Full statistics: " + _statisticsFor->variants[variant];
            if (ImGui::CollapsingHeader(label.c_str())) {
                ImGui::PushID(static_cast<int>(variant));
                Summary(_variantStatistics[variant]);
                ImGui::PopID();
            }
        }
        if (ImGui::BeginTable("repeats", 6,
                              ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_ScrollX)) {
            for (const auto* label : {"Variant", "Repeat", "FPS", "Median ms", "P95 ms", "GPU mean ms"})
                ImGui::TableSetupColumn(label);
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < _runStatistics.size(); ++i) {
                const auto& run = _statisticsFor->runs[i];
                const auto& stats = _runStatistics[i];
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(_statisticsFor->variants[static_cast<std::size_t>(run.variant)].c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", run.repeat + 1);
                ImGui::TableNextColumn();
                ImGui::Text("%.1f", stats.fps);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", stats.frame.median);
                ImGui::TableNextColumn();
                ImGui::Text("%.3f", stats.frame.p95);
                ImGui::TableNextColumn();
                if (stats.gpu.count)
                    ImGui::Text("%.3f", stats.gpu.mean);
                else
                    ImGui::TextUnformatted("Unavailable");
            }
            ImGui::EndTable();
        }
        Frames(*snapshot.profiler.result);
    }
}
void ProfilerPanel::Compare(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    ImGui::InputText("Baseline filename", _filename.data(), _filename.size());
    ImGui::BeginDisabled(snapshot.profiler.busy);
    ImGui::BeginDisabled(!snapshot.profiler.result);
    if (ImGui::Button("Save Baseline")) actions.profiler(EditorProfilerCommand::SaveBaseline, {}, _filename.data());
    ContinueEditorToolbar("Export");
    if (ImGui::Button("Export")) ImGui::OpenPopup("export-report");
    if (ImGui::BeginPopup("export-report")) {
        if (ImGui::MenuItem("JSON")) actions.profiler(EditorProfilerCommand::ExportJson, {}, _filename.data());
        if (ImGui::MenuItem("Markdown")) actions.profiler(EditorProfilerCommand::ExportMarkdown, {}, _filename.data());
        ImGui::EndPopup();
    }
    ImGui::EndDisabled();
    if (ImGui::Button("Compare with Baseline"))
        actions.profiler(EditorProfilerCommand::CompareBaseline, {}, _filename.data());
    ImGui::EndDisabled();
    if (!snapshot.profiler.file.empty()) ImGui::TextWrapped("%s", snapshot.profiler.file.c_str());
    if (snapshot.profiler.baseline) Comparison(*snapshot.profiler.baseline);
    if (snapshot.profiler.result) {
        const auto& metadata = snapshot.profiler.result->metadata;
        ImGui::TextWrapped("%s | %dx%d | %s", metadata.scenario.c_str(), metadata.width, metadata.height,
                           metadata.build.c_str());
        ImGui::TextWrapped("GPU: %s", metadata.gpu.c_str());
        ImGui::TextWrapped("CPU: %s | binary: %s", metadata.cpu.c_str(), metadata.binary.c_str());
    }
}
void ProfilerPanel::Frames(const Profiler::Capture& capture) {
    if (capture.runs.empty()) return;
    _run = std::clamp(_run, 0, static_cast<int>(capture.runs.size() - 1));
    ImGui::SliderInt("Run", &_run, 0, static_cast<int>(capture.runs.size() - 1));
    const auto& run = capture.runs[static_cast<std::size_t>(_run)];
    if (run.frames.empty()) return;
    ImGui::Text("%s | repeat %d", capture.variants[static_cast<std::size_t>(run.variant)].c_str(), run.repeat + 1);
    const auto getter = [](void* data, int index) {
        return static_cast<float>(
            (*static_cast<const std::vector<Profiler::FrameSample>*>(data))[static_cast<std::size_t>(index)]
                .intervalMs);
    };
    ImGui::PlotLines("Frame ms", getter, const_cast<std::vector<Profiler::FrameSample>*>(&run.frames),
                     static_cast<int>(run.frames.size()), 0, nullptr, 0, FLT_MAX, {0, ImGui::GetFontSize() * 6});
    _frame = std::clamp(_frame, 0, static_cast<int>(run.frames.size() - 1));
    ImGui::SliderInt("Frame", &_frame, 0, static_cast<int>(run.frames.size() - 1));
    const auto& frame = run.frames[static_cast<std::size_t>(_frame)];
    ImGui::Text("Frame %.3f ms", frame.intervalMs);
    if (frame.gpuMs)
        ImGui::Text("GPU %.3f ms", *frame.gpuMs);
    else
        ImGui::TextDisabled("GPU unavailable for this frame");
    if (ImGui::BeginTable("selected-frame", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH)) {
        ImGui::TableSetupColumn("CPU scope");
        ImGui::TableSetupColumn("Inclusive ms");
        ImGui::TableSetupColumn("Self ms");
        ImGui::TableHeadersRow();
        for (const auto& scope : frame.scopes) {
            if (scope.id >= capture.scopes.size()) continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(capture.scopes[scope.id].c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", scope.inclusiveMs);
            ImGui::TableNextColumn();
            ImGui::Text("%.3f", scope.selfMs);
        }
        ImGui::EndTable();
    }
}
