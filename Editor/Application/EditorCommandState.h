#pragma once
#include "Editor/Application/EditorPresentation.h"

// Host state only: no worker protocol or UI dependency.
struct EditorCommandContext {
    bool hasProject = false;
    bool hasBuild = false; // A successful build exists to start a scene from
    bool hasSelection = false;
    bool workerActive = false; // A catalog or worker process runs
    bool starting = false;     // A scene is starting and not ready yet
    bool building = false;     // Build & Refresh builds; the running scene stays usable
    bool replacing = false;    // The new build starts beside the running scene and will replace it
    bool ready = false;
    bool paused = false;
    bool profilerBusy = false;
    EditorPhase phase = EditorPhase::Stopped;
    bool benchmarkCancelPending = false;
};

[[nodiscard]] EditorCapabilities ResolveEditorCapabilities(const EditorCommandContext& context);
[[nodiscard]] EditorCommand ResolveEditorPrimaryCommand(EditorPhase phase, const EditorCapabilities& capabilities,
                                                        bool profilerBusy);
