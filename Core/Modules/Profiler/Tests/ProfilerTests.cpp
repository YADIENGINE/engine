#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <limits>
#include "Modules/Profiler/Profiler.hpp"
#include "Tests/Support/TempDirectory.h"

namespace {
Profiler::Capture Samples() {
    Profiler::Capture capture;
    capture.completed = true;
    capture.metadata.scenario = "test.scenario";
    capture.metadata.environment = "test-machine";
    capture.metadata.recipe = {2, 4, 3, 1.0 / 60.0};
    for (int repeat = 0; repeat < 3; ++repeat)
        for (int variant = 0; variant < 2; ++variant) {
            Profiler::Run run{variant, repeat, {}};
            for (int frame = 0; frame < 4; ++frame)
                run.frames.push_back(
                    {static_cast<std::uint64_t>(frame + 1), variant ? 8.0 : 10.0, std::nullopt, {}, {}});
            capture.runs.push_back(std::move(run));
        }
    return capture;
}
} // namespace
TEST_CASE("Profiler.Statistics.InterpolatedPercentilesAndFps", "[engine][profiler]") {
    const auto s = Profiler::Describe({1, 2, 3, 4});
    CHECK(s.count == 4);
    CHECK(s.mean == Catch::Approx(2.5));
    CHECK(s.median == Catch::Approx(2.5));
    CHECK(s.p95 == Catch::Approx(3.85));
    CHECK(s.p99 == Catch::Approx(3.97));
    CHECK(s.maximum == 4);
    auto capture = Samples();
    CHECK(Profiler::Summarize(capture, 0).fps == Catch::Approx(100));
    CHECK(Profiler::Summarize(capture, 1).fps == Catch::Approx(125));
    CHECK(Profiler::Describe({}).count == 0);
}
TEST_CASE("Profiler.Samples.BoundsMissingGpuAndCounters", "[engine][profiler]") {
    Profiler profiler(2);
    const auto counter = profiler.RegisterCounter("requests");
    profiler.RecordFrame({1, 10, std::nullopt, {}, {}});
    profiler.RecordFrame({2, 10, 3.0, {}, {{counter, 4}}});
    profiler.RecordFrame({3, 10, std::nullopt, {}, {}});
    CHECK(profiler.DroppedFrames() == 1);
    auto frames = profiler.TakeFrames();
    REQUIRE(frames.size() == 2);
    CHECK(frames[0].id == 2);
    Profiler::Capture capture;
    capture.counters = profiler.CounterNames();
    capture.runs = {{1, 0, std::move(frames)}};
    const auto summary = Profiler::Summarize(capture);
    CHECK(summary.gpu.count == 1);
    CHECK(summary.gpu.mean == 3);
    CHECK(summary.missingGpu == 1);
    REQUIRE(summary.counters.size() == 1);
    CHECK(summary.counters[0].inclusive.mean == 2);
    CHECK_THROWS(profiler.RecordFrame({4, -1, std::nullopt, {}, {}}));
    CHECK_THROWS(Profiler::Describe({std::numeric_limits<double>::infinity()}));
    CHECK_THROWS(profiler.RecordFrame({4, 1, std::nullopt, {}, {{counter, -1}}}));
}
TEST_CASE("Profiler.Scopes.NestedInclusiveAndSelf", "[engine][profiler]") {
    Profiler profiler;
    const auto outer = profiler.RegisterScope("outer"), inner = profiler.RegisterScope("inner");
    const auto counter = profiler.RegisterCounter("count");
    CHECK(profiler.RegisterScope("outer") == outer);
    profiler.BeginFrame();
    {
        auto a = profiler.Measure(outer);
        {
            auto b = profiler.Measure(inner);
            profiler.RecordCounter(counter, 1);
        }
    }
    profiler.EndFrame();
    auto frames = profiler.TakeFrames();
    REQUIRE(frames.size() == 1);
    REQUIRE(frames[0].scopes.size() == 2);
    const auto& child = frames[0].scopes[0];
    const auto& parent = frames[0].scopes[1];
    CHECK(parent.id == outer);
    CHECK(child.id == inner);
    CHECK(parent.inclusiveMs >= child.inclusiveMs);
    CHECK(parent.selfMs <= parent.inclusiveMs);
    CHECK(parent.inclusiveMs - parent.selfMs == Catch::Approx(child.inclusiveMs));
    REQUIRE(frames[0].counters.size() == 1);
    CHECK(frames[0].counters[0].value == 1);
}
TEST_CASE("Profiler.Reports.RoundTripAndCompatibility", "[engine][profiler]") {
    TempDirectory temporary;
    auto capture = Samples();
    Profiler::Save(temporary.Path() / "capture.json", capture);
    auto loaded = Profiler::Load(temporary.Path() / "capture.json");
    CHECK(Profiler::Encode(capture) == Profiler::Encode(loaded));
    const auto comparison = Profiler::Compare(capture, loaded, 0, 1);
    CHECK(comparison.compatible);
    CHECK(comparison.improvementPercent == Catch::Approx(20));
    CHECK(comparison.assessment == "Measured improvement");
    loaded.metadata.binary = "another-binary";
    CHECK(Profiler::Compare(capture, loaded, 0, 1).compatible);
    loaded.metadata.graphics = "other-settings";
    CHECK_FALSE(Profiler::Compare(capture, loaded, 0, 1).compatible);
    auto malformed = Profiler::Encode(capture);
    malformed["metadata"]["frames"] = std::int64_t{4294967300LL};
    CHECK_THROWS(Profiler::Decode(malformed));
    CHECK(Profiler::Markdown(capture).find("GPU samples") != std::string::npos);
}
TEST_CASE("Profiler.Comparison.UncertainIncompleteAndTailRegression", "[engine][profiler]") {
    auto capture = Samples();
    for (auto& run : capture.runs)
        if (run.variant == 1)
            for (auto& frame : run.frames)
                frame.intervalMs = 9.9;
    CHECK(Profiler::Compare(capture, capture, 0, 1).assessment == "Uncertain");
    for (auto& run : capture.runs)
        if (run.variant == 1) {
            for (auto& frame : run.frames)
                frame.intervalMs = 5;
            run.frames.back().intervalMs = 20;
        }
    CHECK(Profiler::Compare(capture, capture, 0, 1).assessment == "Faster median; P95 regression");
    capture.completed = false;
    CHECK_FALSE(Profiler::Compare(capture, capture, 0, 1).compatible);
    capture = Samples();
    capture.runs.front().frames.pop_back();
    CHECK_FALSE(Profiler::Compare(capture, capture, 0, 1).compatible);
}
TEST_CASE("Profiler.Recipe.AlternatesOrderAndRejectsUnboundedWork", "[engine][profiler]") {
    CHECK(Profiler::RunOrder({}) == std::vector<int>{0, 1, 1, 0, 0, 1});
    CHECK_THROWS(Profiler::RunOrder({0, 0, 1, 1.0 / 60.0}));
    CHECK_THROWS(Profiler::RunOrder({0, 10001, 1, 1.0 / 60.0}));
}
