#include <catch2/catch_test_macros.hpp>
#include <array>
#include <string_view>
#include "Editor/Application/EditorCommandState.h"

namespace {
EditorCommandContext ReadyPreview() {
    EditorCommandContext context;
    context.hasProject = context.hasBuild = context.hasSelection = context.workerActive = context.ready = true;
    context.phase = EditorPhase::Ready;
    return context;
}
} // namespace

TEST_CASE("Editor.Commands.OpenRequiresProjectBuildAndSelection", "[engine-editor]") {
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
    CHECK_FALSE(capabilities.Can(EditorCommand::Open)); // Nothing built yet
    context.hasBuild = true;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Open));
    CHECK(std::string_view(capabilities.Reason(EditorCommand::Open)).empty());
}

TEST_CASE("Editor.Commands.StepRequiresPauseAndBenchmarkDisablesPlayback", "[engine-editor]") {
    auto context = ReadyPreview();
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
    context.profilerBusy = false;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Step));
    CHECK(capabilities.playbackEnabled);
}

TEST_CASE("Editor.Commands.RunningPreviewStaysUsableWhileBuilding", "[engine-editor]") {
    auto context = ReadyPreview();
    context.building = true;
    auto capabilities = ResolveEditorCapabilities(context);
    for (const auto command : std::array{EditorCommand::Pause, EditorCommand::Reset, EditorCommand::Open,
                                         EditorCommand::Stop, EditorCommand::Cancel})
        CHECK(capabilities.Can(command));
    CHECK(capabilities.playbackEnabled);
    CHECK_FALSE(capabilities.Can(EditorCommand::Refresh));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Stop);

    context.building = false;
    context.replacing = true; // The new build starts beside it: hold the controls until the swap
    capabilities = ResolveEditorCapabilities(context);
    for (const auto command :
         std::array{EditorCommand::Pause, EditorCommand::Reset, EditorCommand::Open, EditorCommand::Refresh})
        CHECK_FALSE(capabilities.Can(command));
    CHECK_FALSE(capabilities.playbackEnabled);
    CHECK(capabilities.Can(EditorCommand::Cancel));
}

TEST_CASE("Editor.Commands.LoadingAndBuildOfferCancellationInsteadOfOpen", "[engine-editor]") {
    EditorCommandContext context;
    context.hasProject = context.hasBuild = context.hasSelection = context.workerActive = true;
    context.phase = EditorPhase::ReadingCatalog;
    auto capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Cancel));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Cancel);

    context.phase = EditorPhase::Starting;
    context.starting = true;
    capabilities = ResolveEditorCapabilities(context);
    CHECK_FALSE(capabilities.Can(EditorCommand::Open));
    CHECK(capabilities.Can(EditorCommand::Cancel));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Cancel);

    context.workerActive = context.starting = false;
    context.building = true;
    context.phase = EditorPhase::Building;
    capabilities = ResolveEditorCapabilities(context);
    CHECK(capabilities.Can(EditorCommand::Cancel));
    CHECK(capabilities.Can(EditorCommand::Stop));
    CHECK_FALSE(capabilities.Can(EditorCommand::Refresh));
    CHECK(ResolveEditorPrimaryCommand(context.phase, capabilities, false) == EditorCommand::Cancel);
}
