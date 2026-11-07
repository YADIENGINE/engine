#pragma once
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Session/TuningWorkspace.h"

// An input map in the Inspector (Docs/InputMap.md): one row per button, one column per player, each cell the keys that
// press it. A key is removed by clicking it; `+` waits for the next key pressed (Escape cancels). Buttons and axes stay
// in the file: a new list changes the generated enums and needs a build.
class InputMapInspector {
public:
    void File(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions);

private:
    struct Capture {
        std::filesystem::path source;
        std::size_t player = 0;
        std::string button;
        std::string clicked; // A mouse button clicked on the prompt
    };
    void Status(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void Cell(const TuningFileState& file, std::size_t player, std::size_t button, const EditorUiActions& actions);
    // While waiting for a key: the next key pressed, if the input map knows it.
    void Listen(const TuningFileState& file, const EditorUiActions& actions);

    std::optional<Capture> _capture;
};
