#include "Editor/Application/EditorCommandState.h"

EditorCapabilities ResolveEditorCapabilities(const EditorCommandContext& context) {
    EditorCapabilities result;
    // Preview controls lock only while the preview itself is not ready to take them; a build does not lock them.
    const bool locked = context.profilerBusy || context.starting || context.replacing;
    const bool benchmarkCancelPending = context.profilerBusy && context.benchmarkCancelPending;
    const auto set = [&](EditorCommand command, bool enabled, const char* reason) {
        const auto index = static_cast<std::size_t>(command);
        result.enabled[index] = enabled;
        result.disabledReasons[index] = enabled ? "" : reason;
    };
    const char* operationReason = context.profilerBusy ? "Cancel the benchmark first."
                                  : context.replacing  ? "Wait for the new build to replace the preview."
                                  : context.starting   ? "Wait for the preview to start."
                                  : context.phase == EditorPhase::ReadingCatalog
                                      ? "Wait for the catalog to load."
                                      : "Wait for the current operation to finish.";
    const char* previewReason = locked ? operationReason : "Open a preview first.";
    const bool canControl = context.ready && !locked;
    set(EditorCommand::Open, context.hasProject && context.hasBuild && context.hasSelection && !locked,
        !context.hasProject     ? "No project configured."
        : !context.hasBuild     ? "Build the preview target first (Build & Refresh)."
        : !context.hasSelection ? "Select a preview."
                                : operationReason);
    set(EditorCommand::Stop, context.workerActive || context.building, "No preview is running.");
    set(EditorCommand::Pause, canControl, previewReason);
    set(EditorCommand::Reset, canControl, previewReason);
    set(EditorCommand::Refresh, context.hasProject && !context.building && !locked,
        !context.hasProject ? "No project configured."
        : context.building  ? "A build is already running."
                            : operationReason);
    const bool cancellable = context.profilerBusy || context.building || context.replacing || context.starting ||
                             (context.workerActive && context.phase == EditorPhase::ReadingCatalog);
    set(EditorCommand::Cancel, cancellable && !benchmarkCancelPending,
        benchmarkCancelPending ? "Waiting for benchmark cancellation." : "No operation to cancel.");
    set(EditorCommand::Step, canControl && context.paused, canControl ? "Pause the preview first." : previewReason);
    result.playbackEnabled = canControl;
    result.playbackDisabledReason = canControl ? "" : previewReason;
    return result;
}

EditorCommand ResolveEditorPrimaryCommand(EditorPhase phase, const EditorCapabilities& capabilities,
                                          bool profilerBusy) {
    if (profilerBusy || phase == EditorPhase::ReadingCatalog || phase == EditorPhase::Starting ||
        phase == EditorPhase::Building)
        return EditorCommand::Cancel;
    if (phase == EditorPhase::Ready || phase == EditorPhase::Paused || capabilities.Can(EditorCommand::Stop))
        return EditorCommand::Stop;
    return EditorCommand::Open;
}
