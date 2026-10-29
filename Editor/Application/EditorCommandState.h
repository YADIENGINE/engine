#pragma once
#include "Editor/Application/EditorPresentation.h"

// Host state only: no worker protocol or UI dependency.
struct EditorCommandContext {
    bool hasProject = false;
    bool hasSelection = false;
    bool workerActive = false;
    bool buildActive = false;
    bool refreshBusy = false;
    bool stopping = false;
    bool ready = false;
    bool paused = false;
    bool profilerBusy = false;
    EditorPhase phase = EditorPhase::Stopped;
    bool benchmarkCancelPending = false;
};

[[nodiscard]] EditorCapabilities ResolveEditorCapabilities(const EditorCommandContext& context);
[[nodiscard]] EditorCommand ResolveEditorPrimaryCommand(EditorPhase phase, const EditorCapabilities& capabilities,
                                                        bool profilerBusy);
