#pragma once
#include "Editor/Application/EditorPresentation.h"

// Host state only: no worker protocol or UI dependency.
struct EditorCommandContext {
    bool hasProject = false;
    bool hasBuild = false; // A successful build exists to start a preview from
    bool hasSelection = false;
    bool workerActive = false; // A catalog or preview worker runs
    bool starting = false;     // A preview is starting and not ready yet
    bool building = false;     // Build & Refresh builds; the running preview stays usable
    bool replacing = false;    // The new build starts beside the running preview and will replace it
    bool ready = false;
    bool paused = false;
    bool profilerBusy = false;
    EditorPhase phase = EditorPhase::Stopped;
    bool benchmarkCancelPending = false;
};

[[nodiscard]] EditorCapabilities ResolveEditorCapabilities(const EditorCommandContext& context);
[[nodiscard]] EditorCommand ResolveEditorPrimaryCommand(EditorPhase phase, const EditorCapabilities& capabilities,
                                                        bool profilerBusy);
