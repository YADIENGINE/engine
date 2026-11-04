#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include <imgui.h>
#include <algorithm>
#include "Editor/Internals/Session/AgentContext.h"
#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr std::size_t SearchThreshold = 6;
constexpr float MinimumSearchWidth = 100.0f;

std::string ApplyHint(const SceneParameter& parameter, const std::vector<SceneAction>& actions) {
    switch (parameter.apply) {
    case SceneApplyMode::Immediate : return {};
    case SceneApplyMode::OnReset   : return "Applies on toolbar Reset.";
    case SceneApplyMode::NextAction: {
        const auto action = std::ranges::find(actions, parameter.applyAction, &SceneAction::id);
        return action == actions.end() ? "Applies on the next scene action." : "Applies on " + action->label + ".";
    }
    }
    return {};
}

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}

// The tuning files the running scene reads, in the project's file order.
std::vector<const TuningFileState*> UsedFiles(const EditorSnapshot& snapshot) {
    std::vector<const TuningFileState*> used;
    if (snapshot.tuning)
        for (const auto& file : snapshot.tuning->Files())
            if (file.used) used.push_back(&file);
    return used;
}
} // namespace

void InspectorPanel::ResetFilters() {
    _contextParameter.clear();
    _search.fill('\0');
    _modifiedOnly = false;
    _searchVisible = false;
}

void InspectorPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    BeginInspectorStyle();
    const auto* file = snapshot.tuning && !snapshot.inspectedTuning.empty()
                           ? snapshot.tuning->Find(snapshot.inspectedTuning)
                           : nullptr;
    if (file) {
        File(*file, snapshot, actions);
    } else if (snapshot.selected) {
        Scene(snapshot, actions);
    } else {
        if (snapshot.tuning && !snapshot.tuning->Files().empty()) FilePicker(snapshot, actions, "Tuning files...");
        ToolEmptyState("No scene selected", "Pick a scene to see its controls and the tuning it reads, or open a "
                                            "tuning file above.");
    }
    _tuning.Finish(actions);
    _tuning.Shortcuts(snapshot, actions);
    EndInspectorStyle();
}

void InspectorPanel::FilePicker(const EditorSnapshot& snapshot, const EditorUiActions& actions, const char* preview) {
    ImGui::SetNextItemWidth(-1);
    if (!ToolBeginCombo("##tuning-files", preview)) {
        ToolTooltip("Open one of the project's tuning files in the Inspector.");
        return;
    }
    for (const auto& file : snapshot.tuning->Files()) {
        std::string label = file.label;
        if (file.problem)
            label += "  (invalid)";
        else if (file.target.empty())
            label += "  (not built)";
        else if (file.used)
            label += "  (live)";
        if (ToolWrappedSelectable((label + "##" + Utf8(file.source)).c_str(), file.source == snapshot.inspectedTuning,
                                  ImGuiSelectableFlags_None))
            actions.inspectTuning(file.source);
    }
    ImGui::EndCombo();
}

void InspectorPanel::Filters(std::size_t count, std::size_t modified) {
    const std::string modifiedLabel = "Modified (" + std::to_string(modified) + ")###modified";
    const float gap = ImGui::GetStyle().ItemSpacing.x;
    ImGui::PushFont(ToolFont(ToolFontRole::Medium), ToolMetrics::BodySize); // As ToolSegment draws it
    const float filterWidth =
        ImGui::CalcTextSize(modifiedLabel.c_str(), nullptr, true).x + ImGui::GetStyle().FramePadding.x * 2;
    ImGui::PopFont();
    const bool compactSearch = count <= SearchThreshold;
    bool focusSearch = false;
    if (compactSearch) {
        if (ToolSegment("Search", _searchVisible)) {
            _searchVisible = !_searchVisible;
            if (!_searchVisible) _search.fill('\0');
            focusSearch = _searchVisible;
        }
        if (ImGui::GetContentRegionAvail().x >= ImGui::GetItemRectSize().x + gap + filterWidth) ImGui::SameLine();
    }
    const bool searchShown = !compactSearch || _searchVisible;
    const bool inlineSearch =
        !compactSearch && ImGui::GetContentRegionAvail().x >= MinimumSearchWidth * ToolUiScale() + filterWidth + gap;
    if (inlineSearch) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - filterWidth - gap);
        ImGui::InputTextWithHint("##property-search", "Search properties...", _search.data(), _search.size());
        ImGui::SameLine();
    }
    if (ToolSegment(modifiedLabel.c_str(), _modifiedOnly)) _modifiedOnly = !_modifiedOnly;
    ToolTooltip("Show only properties that differ: scene controls from their defaults, tuning values from the last "
                "commit.");
    if (searchShown && !inlineSearch) {
        ImGui::SetNextItemWidth(-1);
        if (focusSearch) ImGui::SetKeyboardFocusHere();
        ImGui::InputTextWithHint("##property-search", "Search properties...", _search.data(), _search.size());
    }
}

void InspectorPanel::File(const TuningFileState& file, const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const auto key = Utf8(file.source);
    const bool changed = _shown != key;
    if (changed) {
        _shown = key;
        ResetFilters();
    }
    ImGui::PushID(key.c_str());
    if (snapshot.selected) {
        if (ToolButton(("< " + snapshot.selected->name).c_str())) actions.inspectTuning({});
        ToolTooltip("Back to the selected scene.");
        ImGui::SameLine();
    }
    if (ToolButton("Open in code editor")) actions.openSource(key, 1);
    InspectorSummary(file.label.c_str(), ("Saved in " + file.relative +
                                          ". Values apply live and are compiled into Release builds; Ctrl+Z undoes "
                                          "an edit made here.")
                                             .c_str());
    FilePicker(snapshot, actions, file.label.c_str());
    Filters(file.fields.size(), TuningInspector::ModifiedCount(file));
    const auto query = EditorSearchText(_search.data());
    if (ImGui::BeginChild("##inspector-content")) {
        if (changed) ImGui::SetScrollY(0);
        _tuning.File(file, snapshot, actions, query, _modifiedOnly, true);
        if ((!query.empty() || _modifiedOnly) && !TuningInspector::Shows(file, query, _modifiedOnly))
            ToolEmptyState("No matching properties", "Change the search or turn off Modified only.");
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void InspectorPanel::Scene(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    const auto& selected = *snapshot.selected;
    const bool changedScene = _shown != selected.id;
    if (changedScene) {
        _shown = selected.id;
        ResetFilters();
    }
    const auto hidden = [&](const std::string& id) {
        return id == "language" || std::ranges::find(snapshot.inspection.hiddenParameters, id) !=
                                       snapshot.inspection.hiddenParameters.end();
    };
    const auto disabled = [&](const std::string& id) {
        return std::ranges::find(snapshot.inspection.disabledParameters, id) !=
               snapshot.inspection.disabledParameters.end();
    };
    const auto used = UsedFiles(snapshot);
    ImGui::PushID(selected.id.c_str());
    InspectorSummary(selected.name.c_str(), selected.description.c_str());
    std::size_t visibleCount = 0;
    std::size_t modifiedCount = 0;
    float labelWidth = 0;
    for (const auto& parameter : selected.parameters) {
        const auto found = snapshot.parameters.find(parameter.id);
        if (hidden(parameter.id) || found == snapshot.parameters.end()) continue;
        ++visibleCount;
        if (found->second != parameter.initial) ++modifiedCount;
        if (!parameter.choiceButtons || InspectorChoiceFitsRow(parameter))
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(parameter.label.c_str()).x);
    }
    for (const auto* file : used) {
        visibleCount += file->fields.size();
        modifiedCount += TuningInspector::ModifiedCount(*file);
    }
    for (const auto& value : snapshot.inspection.values)
        labelWidth = std::max(labelWidth, ImGui::CalcTextSize(value.label.c_str()).x);
    labelWidth += ImGui::GetFontSize();
    Filters(visibleCount, modifiedCount);
    const auto query = EditorSearchText(_search.data());
    const auto matches = [&](const SceneParameter& parameter) {
        if (hidden(parameter.id)) return false;
        const auto found = snapshot.parameters.find(parameter.id);
        if (found == snapshot.parameters.end()) return false;
        if (_modifiedOnly && found->second == parameter.initial) return false;
        return query.empty() || EditorSearchText(parameter.label + " " + parameter.id + " " + parameter.group + " " +
                                                 parameter.description)
                                        .find(query) != std::string::npos;
    };
    std::vector<std::string> groups;
    for (const auto& parameter : selected.parameters) {
        if (!matches(parameter)) continue;
        const auto group = parameter.group.empty() ? "General" : parameter.group;
        if (std::ranges::find(groups, group) == groups.end()) groups.push_back(group);
    }
    const auto primary = std::ranges::find(selected.actions, true, &SceneAction::primary);
    const float footerHeight = primary == selected.actions.end()
                                   ? 0
                                   : ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2 + 1;
    const float contentHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight);
    const bool drawContent = ImGui::BeginChild("##inspector-content", {0, contentHeight});
    if (changedScene) ImGui::SetScrollY(0);
    if (drawContent) {
        bool openContext = false;
        ImGui::BeginDisabled(!snapshot.ready);
        for (const auto& group : groups) {
            bool advanced = true;
            for (const auto& parameter : selected.parameters) {
                if (hidden(parameter.id) || !snapshot.parameters.contains(parameter.id) ||
                    (parameter.group.empty() ? "General" : parameter.group) != group)
                    continue;
                advanced = advanced && parameter.advanced;
            }
            if (!InspectorSection(group.c_str(), !query.empty() || _modifiedOnly, !advanced)) continue;
            ToolTooltip("Scene controls: kept for this session, never saved.");
            ImGui::PushID(group.c_str());
            const float contentWidth = ImGui::GetContentRegionAvail().x;
            bool properties = false;
            for (const auto& parameter : selected.parameters) {
                if (!matches(parameter)) continue;
                if ((parameter.group.empty() ? "General" : parameter.group) != group) continue;
                const auto found = snapshot.parameters.find(parameter.id);
                if (found == snapshot.parameters.end()) continue;
                const bool choiceGrid = InspectorChoiceIsBlock(parameter, labelWidth, contentWidth);
                if (choiceGrid && properties) {
                    EndInspectorProperties();
                    properties = false;
                }
                if (!choiceGrid && !properties) {
                    properties = BeginInspectorProperties(parameter.id.c_str(), labelWidth);
                    if (!properties) continue;
                }
                ImGui::PushID(parameter.id.c_str());
                ImGui::BeginDisabled(disabled(parameter.id));
                auto value = found->second;
                const auto hint = ApplyHint(parameter, selected.actions);
                bool contextRequested = false;
                if (InspectorParameterField(parameter, value, snapshot.pending.contains(parameter.id), hint.c_str(),
                                            &contextRequested))
                    actions.parameter(parameter.id, value);
                if (contextRequested) {
                    _contextParameter = parameter.id;
                    openContext = true;
                }
                ImGui::EndDisabled();
                ImGui::PopID();
            }
            if (properties) EndInspectorProperties();
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        // The tuning files the scene has read: saved values, which stay editable whatever the scene does.
        bool tuningShown = false;
        for (const auto* file : used) {
            if (!TuningInspector::Shows(*file, query, _modifiedOnly)) continue;
            tuningShown = true;
            if (!InspectorSection((file->label + "###tuning." + Utf8(file->source)).c_str(),
                                  !query.empty() || _modifiedOnly))
                continue;
            ToolTooltip(("Tuning file: saved in " + file->relative + ". Ctrl+Z undoes an edit made here.").c_str());
            _tuning.File(*file, snapshot, actions, query, _modifiedOnly, false);
        }
        if (groups.empty() && !tuningShown && (_search.front() != '\0' || _modifiedOnly))
            ToolEmptyState("No matching properties", "Change the search or turn off Modified only.");
        if (query.empty() && !_modifiedOnly && snapshot.tuning && !snapshot.tuning->Files().empty())
            FilePicker(snapshot, actions, used.empty() ? "Tuning files..." : "Other tuning files...");
        ImGui::BeginDisabled(!snapshot.ready);
        const bool secondary =
            std::ranges::any_of(selected.actions, [](const auto& action) { return !action.primary; });
        if (secondary && InspectorSection("Actions"))
            InspectorActionGroup(selected.actions, snapshot.inspection.disabledActions, actions.action, false);
        ImGui::EndDisabled();
        if (openContext) ImGui::OpenPopup("parameter-context");
        if (ImGui::BeginPopup("parameter-context")) {
            const auto parameter = std::ranges::find(selected.parameters, _contextParameter, &SceneParameter::id);
            const auto value = snapshot.parameters.find(_contextParameter);
            if (parameter != selected.parameters.end() && value != snapshot.parameters.end()) {
                if (ImGui::MenuItem("Send to Agent"))
                    _agentText = ParameterContext(selected, *parameter, value->second);
                ToolTooltip("Paste this parameter's value, range or choices and description into the agent session.");
            }
            ImGui::EndPopup();
        }
        if (!snapshot.inspection.values.empty() && InspectorSection("Live information")) {
            if (BeginInspectorProperties("##live", labelWidth, true)) {
                for (const auto& value : snapshot.inspection.values)
                    InspectorReadOnlyField(value);
                EndInspectorProperties();
            }
        }
    }
    ImGui::EndChild();
    if (primary != selected.actions.end()) {
        ImGui::Separator();
        ImGui::BeginDisabled(!snapshot.ready);
        InspectorPrimaryAction(*primary, snapshot.inspection.disabledActions, actions.action);
        ImGui::EndDisabled();
    }
    ImGui::PopID();
}
