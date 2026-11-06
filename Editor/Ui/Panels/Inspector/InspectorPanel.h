#pragma once
#include <array>
#include <string>
#include <utility>
#include "Editor/Application/EditorPresentation.h"
#include "Editor/Ui/Panels/Inspector/InputMapInspector.h"
#include "Editor/Ui/Panels/Inspector/TuningInspector.h"

// The selected scene's controls and the tuning files it reads, or one file picked on its own: a tuning file, the
// project's settings or an input map (Docs/Editor/Usage.md → Inspector). Scene controls last for the session; the
// others are saved in their files.
class InspectorPanel {
public:
    void Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    [[nodiscard]] std::string TakeAgentText() {
        auto text = std::exchange(_agentText, {});
        return text.empty() ? _tuning.TakeAgentText() : text;
    }

private:
    void Scene(const EditorSnapshot& snapshot, const EditorUiActions& actions);
    void File(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions);
    // Search and the Modified filter; `count` fields are visible, `modified` of them differ.
    void Filters(std::size_t count, std::size_t modified);
    // A list of the project's tuning files; picking one shows it in the Inspector.
    void FilePicker(const EditorSnapshot& snapshot, const EditorUiActions& actions, const char* preview);
    void ResetFilters(bool hideSearch = false);
    void EmptyResults();

    TuningInspector _tuning;
    InputMapInspector _inputMap;
    std::array<char, 128> _search{};
    std::string _shown; // The scene id or tuning file whose filters are set
    std::string _contextParameter;
    std::string _agentText;
    bool _modifiedOnly = false;
    bool _searchVisible = false;
    bool _focusSearch = false;
};
