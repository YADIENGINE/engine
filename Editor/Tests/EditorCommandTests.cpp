#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string_view>
#include "Editor/Application/EditorCommandState.h"

TEST_CASE("Editor.Commands.StoppedSessionRequiresSelectionAndProject", "[engine-editor]") {
    EditorCommandContext context;
    auto capabilities = ResolveEditorCapabilities(context);
    CHECK_FALSE(capabilities.Can(EditorCommand::Open));
    CHECK_FALSE(capabilities.Can(EditorCommand::Refresh));
    CHECK_FALSE(capabilities.Can(EditorCommand::Cancel));
    CHECK_FALSE(std::string_view(capabilities.Reason(EditorCommand::Open)).empty());
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Open);
    context.hasProject = true;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Refresh));
    CHECK_FALSE(capabilities.Can(EditorCommand::Open));
    context.hasSelection = true;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Open));
    CHECK(std::string_view(capabilities.Reason(EditorCommand::Open)).empty());
}

TEST_CASE("Editor.Commands.StepRequiresPauseAndBenchmarkDisablesPlayback", "[engine-editor]") {
    EditorCommandContext context;
    context.hasProject = context.hasSelection = context.workerActive = context.ready = true;
    context.phase = EditorPhase::Ready;
    auto capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Pause));
    CHECK(capabilities.Can(EditorCommand::Reset));
    CHECK(capabilities.playbackEnabled);
    CHECK_FALSE(capabilities.Can(EditorCommand::Step));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Stop);
    context.paused = true;
    context.phase = EditorPhase::Paused;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Step));
    context.profilerBusy = true;
    capabilities = ResolveEditorCapabilities(context);
    for (const auto command : std::array{EditorCommand::Open, EditorCommand::Pause, EditorCommand::Reset,
                                         EditorCommand::Step, EditorCommand::Refresh})
        CHECK_FALSE(capabilities.Can(command));
    CHECK_FALSE(capabilities.playbackEnabled);
    CHECK_FALSE(std::string_view(capabilities.playbackDisabledReason).empty());
    CHECK(capabilities.Can(EditorCommand::Cancel));
    CHECK(capabilities.Can(EditorCommand::Stop));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, true) == EditorCommand::Cancel);
    context.benchmarkCancelPending = true;
    capabilities = ResolveEditorCapabilities(context);
    CHECK_FALSE(capabilities.Can(EditorCommand::Cancel));
    CHECK_FALSE(std::string_view(capabilities.Reason(EditorCommand::Cancel)).empty());
    CHECK(capabilities.Can(EditorCommand::Stop));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, true) == EditorCommand::Cancel);
    context.profilerBusy = false;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Step));
    CHECK(capabilities.playbackEnabled);
}

TEST_CASE("Editor.Commands.LoadingAndBuildOfferCancellationInsteadOfOpen", "[engine-editor]") {
    EditorCommandContext context;
    context.hasProject = context.hasSelection = context.workerActive = true;
    for (const auto phase : std::array{EditorPhase::ReadingCatalog, EditorPhase::Starting}) {
        CAPTURE(static_cast<int>(phase));
        context.phase = phase;
        const auto capabilities = ResolveEditorCapabilities(context);
        CHECK_FALSE(capabilities.Can(EditorCommand::Open));
        CHECK(capabilities.Can(EditorCommand::Cancel));
        CHECK(ResolveEditorPrimaryCommand(phase, capabilities, false) == EditorCommand::Cancel);
    }
    context.workerActive = false;
    context.buildActive = context.refreshBusy = true;
    context.phase = EditorPhase::Building;
    const auto capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Cancel));
    CHECK_FALSE(capabilities.Can(EditorCommand::Refresh));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Cancel);
}

TEST_CASE("Editor.Commands.StoppingDoesNotRepeatStopOrCancel", "[engine-editor]") {
    EditorCommandContext context;
    context.hasProject = context.hasSelection = true;
    context.phase = EditorPhase::Stopping;
    for (const bool building : std::array{false, true}) {
        CAPTURE(building);
        context.workerActive = !building;
        context.buildActive = building;
        context.stopping = !building;
        const auto capabilities = ResolveEditorCapabilities(context);
        for (const auto command : std::array{EditorCommand::Stop, EditorCommand::Cancel, EditorCommand::Refresh})
            CHECK_FALSE(capabilities.Can(command));
        // Catalog selection sends Select then Open; the latter must queue behind an ordinary worker stop.
        CHECK(capabilities.Can(EditorCommand::Open) == !building);
        CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Stop);
    }
}
