#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Internals/Session/TuningWorkspace.h"

// The tuning part of the Inspector (Docs/Tuning.md → Editor): a file's status and fields, edited with the same property
// widgets as scene parameters. A drag previews in the running scene and writes the file once, when it ends; Modified
// and the reset button compare with the committed file.
class TuningInspector {
public:
    // A file's status and fields. `standalone`: the file fills the Inspector; otherwise it is a section of a scene.
    void File(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions,
              const std::string& query, bool modifiedOnly, bool standalone);
    // Whether the filter leaves any field of the file.
    [[nodiscard]] static bool Shows(const TuningFileState& file, const std::string& query, bool modifiedOnly);
    // Fields that differ from the committed file.
    [[nodiscard]] static std::size_t ModifiedCount(const TuningFileState& file);
    // Once per frame after drawing: a value dragged in this panel is written when nothing is held any more.
    void Finish(const EditorUiActions& actions);
    // Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) while the Inspector has focus and no text is being typed.
    void Shortcuts(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] std::string TakeAgentText() { return std::exchange(_agentText, {}); }

private:
    struct Previewing {
        std::filesystem::path source;
        TuningLocation at;
        TuningValue value;
    };
    void Status(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                bool standalone);
    void Fields(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                const std::string& query, bool modifiedOnly);
    void Scalar(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions);
    void List(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions);
    void Table(const TuningFileState& file, const TuningField& field, const EditorUiActions& actions);
    // A box for one value at `width`; true when it changed this frame (the preview has been sent).
    bool Value(const TuningFileState& file, const TuningLocation& at, const TuningSpec& spec, const TuningValue& stored,
               bool modified, float width, const EditorUiActions& actions);
    void ContextMenu(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void OpenContext(const TuningFileState& file, const TuningField& field);

    std::optional<Previewing> _previewing;
    unsigned int _popup = 0; // ImGuiID of the file being drawn's context menu
    std::filesystem::path _contextFile;
    std::string _contextField;
    std::string _agentText;
};
