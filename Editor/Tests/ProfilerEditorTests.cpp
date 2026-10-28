#include <catch2/catch_test_macros.hpp>
#include "Editor/Internals/Session/BenchmarkState.h"
#include "Editor/Internals/Session/ProfilerSession.h"

namespace {
nlohmann::json Encode(const Profiler::Capture& capture) {
    return nlohmann::json::parse(JsonIO::Serialize(Profiler::Encode(capture)));
}
PreviewMessage Message(std::uint64_t generation, const std::string& type, nlohmann::json data) {
    return {{}, generation, 0, type, std::move(data)};
}
} // namespace
TEST_CASE("Editor.Profiler.WarmupIsSeparateAndOrderRepeats", "[engine-editor][profiler]") {
    BenchmarkState state;
    state.Start({2, 4, 3, 1.0 / 60.0});
    const std::vector<int> expected{0, 1, 1, 0, 0, 1};
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CHECK(state.RunIndex() == i);
        CHECK(state.Variant() == expected[i]);
        state.BeginRun();
        CHECK(state.Phase() == BenchmarkPhase::Warmup);
        CHECK_FALSE(state.Measured(4));
        CHECK_FALSE(state.AddWarmup(1));
        CHECK(state.AddWarmup(1));
        CHECK(state.Phase() == BenchmarkPhase::Measuring);
        CHECK_FALSE(state.Measured(3));
        CHECK(state.Measured(4));
        CHECK(state.FinishRun() == (i + 1 < expected.size()));
    }
    CHECK(state.Phase() == BenchmarkPhase::Completed);
    state.Start({0, 2, 1, 1.0 / 60.0});
    state.BeginRun();
    CHECK(state.Phase() == BenchmarkPhase::Measuring);
    state.Cancel();
    CHECK(state.Phase() == BenchmarkPhase::Cancelled);
    CHECK_FALSE(state.Measured(2));
}
TEST_CASE("Editor.Profiler.RejectsStaleFramesAndAtomicTransfer", "[engine-editor][profiler]") {
    ProfilerSession session;
    session.Reset(3);
    session.Arm(7);
    Profiler::Capture header;
    header.metadata.scenario = "test.live";
    session.Receive(Message(2, "profiler-result",
                            {{"captureId", 7}, {"kind", "live"}, {"runCount", 1}, {"header", Encode(header)}}));
    CHECK_FALSE(session.Snapshot().live);
    session.Receive(Message(3, "profiler-result",
                            {{"captureId", 6}, {"kind", "live"}, {"runCount", 1}, {"header", Encode(header)}}));
    CHECK_FALSE(session.Snapshot().live);
    session.Receive(Message(3, "profiler-result",
                            {{"captureId", 7}, {"kind", "live"}, {"runCount", 1}, {"header", Encode(header)}}));
    header.runs = {{1, 0, {{1, 10, std::nullopt, {}, {}}}}};
    auto chunk = Message(3, "profiler-chunk",
                         {{"captureId", 7}, {"kind", "live"}, {"run", 0}, {"offset", 0}, {"capture", Encode(header)}});
    session.Receive(chunk);
    CHECK_FALSE(session.Snapshot().live);
    CHECK_THROWS(session.Receive(chunk));
    session.Receive(Message(3, "profiler-complete", {{"captureId", 7}, {"kind", "live"}}));
    REQUIRE(session.Snapshot().liveSummary);
    CHECK(session.Snapshot().liveSummary->fps == 100);
    session.Disconnected();
    CHECK_FALSE(session.Snapshot().busy);
    CHECK(session.Snapshot().phase == "Failed");
}

TEST_CASE("Editor.Profiler.ExecutionRestorationOnCancelAndCompletion", "[engine-editor][profiler]") {
    BenchmarkState state;
    const FrameExecutionSettings original{30, true, {GlowBatching::Individual}};
    state.Start({0, 2, 1, 1.0 / 60.0}, original);
    state.BeginRun();
    state.Cancel();
    const auto restored = state.TakeSavedExecution();
    REQUIRE(restored);
    CHECK(restored->fpsLimit == original.fpsLimit);
    CHECK(restored->vsync == original.vsync);
    CHECK(restored->drawing == original.drawing);
    CHECK_FALSE(state.TakeSavedExecution());

    state.Start({0, 2, 1, 1.0 / 60.0}, {60, false, {GlowBatching::Batched}});
    for (int run = 0; run < 2; ++run) {
        state.BeginRun();
        REQUIRE(state.Measured(2));
        (void)state.FinishRun();
    }
    const auto completed = state.TakeSavedExecution();
    REQUIRE(completed);
    CHECK(completed->fpsLimit == 60);
    CHECK_FALSE(completed->vsync);
    CHECK(completed->drawing.glowBatching == GlowBatching::Batched);
    CHECK_FALSE(state.TakeSavedExecution());
}

TEST_CASE("Editor.Profiler.BenchmarkMetadataRoundTripAndValidation", "[engine-editor][profiler]") {
    PreviewDefinition definition;
    definition.id = "test.benchmark";
    definition.name = "Test benchmark";
    definition.parameters = {
        {.id = "enabled", .label = "Enabled", .kind = PreviewParameterKind::Boolean, .initial = true}};
    definition.benchmark = PreviewBenchmark{definition.id};
    definition.benchmark->variants[0].parameters = {{"enabled", false}};
    definition.create = [](const PreviewServices&, const PreviewParameters&) {
        return std::unique_ptr<EditorPreview>{};
    };
    PreviewRegistry registry;
    registry.Add(definition);
    const auto encoded = EncodePreviewCatalog(registry.Entries());
    const auto decoded = DecodePreviewCatalog(encoded);
    REQUIRE(decoded.size() == 1);
    REQUIRE(decoded.front().benchmark);
    CHECK(decoded.front().benchmark->id == definition.id);
    CHECK(decoded.front().benchmark->variants[0].execution.glowBatching == GlowBatching::Individual);
    CHECK(decoded.front().benchmark->variants[1].execution.glowBatching == GlowBatching::Batched);
    CHECK(std::get<bool>(decoded.front().benchmark->variants[0].parameters.at("enabled")) == false);
    definition.id = "test.invalid";
    definition.benchmark->variants[0].parameters = {{"missing", true}};
    CHECK_THROWS(registry.Add(definition));
    auto invalid = encoded;
    invalid["entries"][0]["benchmark"]["variants"][0]["parameters"] = EncodePreviewParameters({{"missing", true}});
    CHECK_THROWS(DecodePreviewCatalog(invalid));
}
