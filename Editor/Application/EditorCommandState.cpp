#include "Editor/Application/EditorCommandState.h"

EditorCapabilities ResolveEditorCapabilities(const EditorCommandContext& context) {
    EditorCapabilities result;
    const bool stopping = context.stopping || context.phase == EditorPhase::Stopping;
    const bool starting = context.workerActive && !context.ready && context.phase != EditorPhase::ReadingCatalog;
    const bool busy = context.profilerBusy || context.buildActive || context.refreshBusy || stopping || starting;
    const bool benchmarkCancelPending = context.profilerBusy && context.benchmarkCancelPending;
    const auto set = [&](EditorCommand command, bool enabled, const char* reason) {
        const auto index = static_cast<std::size_t>(command);
        result.enabled[index] = enabled;
        result.disabledReasons[index] = enabled ? "" : reason;
    };
    const char* operationReason = context.profilerBusy ? "Cancel the benchmark first."
                                  : stopping           ? "Wait for the preview to stop."
                                  : starting           ? "Wait for the preview to start."
                                  : context.phase == EditorPhase::ReadingCatalog
                                      ? "Wait for the catalog to load."
                                      : "Wait for the current operation to finish.";
    const char* previewReason = busy ? operationReason : "Open a preview first.";
    const bool canControl = context.ready && !busy;
    // Selecting another preview queues Open after Select has requested the current worker's shutdown.
    const bool canOpen = context.hasProject && context.hasSelection && !context.profilerBusy && !context.buildActive &&
                         !context.refreshBusy && (!context.workerActive || context.ready || context.stopping);
    set(EditorCommand::Open, canOpen,
        !context.hasProject     ? "No project configured."
        : !context.hasSelection ? "Select a preview."
                                : operationReason);
    set(EditorCommand::Stop, (context.workerActive || context.buildActive) && !stopping,
        stopping ? operationReason : "No preview is running.");
    set(EditorCommand::Pause, canControl, previewReason);
    set(EditorCommand::Reset, canControl, previewReason);
    set(EditorCommand::Refresh, context.hasProject && !busy,
        context.hasProject ? operationReason : "No project configured.");
    set(EditorCommand::Cancel,
        !benchmarkCancelPending && !stopping &&
            (busy || (context.workerActive && context.phase == EditorPhase::ReadingCatalog)),
        benchmarkCancelPending ? "Waiting for benchmark cancellation."
        : stopping             ? operationReason
                               : "No operation to cancel.");
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
    if (phase == EditorPhase::Stopping || phase == EditorPhase::Ready || phase == EditorPhase::Paused ||
        capabilities.Can(EditorCommand::Stop))
        return EditorCommand::Stop;
    return EditorCommand::Open;
}
