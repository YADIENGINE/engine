#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include <imgui.h>
#include <algorithm>
#include "Editor/Ui/Widgets/InspectorWidgets.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

void InspectorPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) {
    BeginInspectorStyle();
    if (!snapshot.selected) {
        EditorEmptyState("No preview selected", "Choose a preview in Project to see its properties and actions.");
        EndInspectorStyle();
        return;
    }
    const auto& selected = *snapshot.selected;
    if (_preview != selected.id) {
        _preview = selected.id;
        _search.fill('\0');
        _modifiedOnly = false;
    }
    const auto hidden = [&](const std::string& id) {
        return id == "language" || std::ranges::find(snapshot.inspection.hiddenParameters, id) !=
                                       snapshot.inspection.hiddenParameters.end();
    };
    const auto disabled = [&](const std::string& id) {
        return std::ranges::find(snapshot.inspection.disabledParameters, id) !=
               snapshot.inspection.disabledParameters.end();
    };
    ImGui::PushID(selected.id.c_str());
    InspectorSummary(selected.name.c_str(), selected.description.c_str());
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##property-search", "Search properties...", _search.data(), _search.size());
    if (EditorSegment("Modified only", _modifiedOnly)) _modifiedOnly = !_modifiedOnly;
    const auto query = EditorSearchText(_search.data());
    const auto matches = [&](const PreviewParameter& parameter) {
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
    ImGui::BeginDisabled(!snapshot.ready);
    for (const auto& group : groups) {
        if (!InspectorSection(group.c_str(), !query.empty() || _modifiedOnly)) continue;
        ImGui::PushID(group.c_str());
        float labelWidth = 0;
        for (const auto& parameter : selected.parameters) {
            if (!matches(parameter) || parameter.choiceButtons ||
                (parameter.group.empty() ? "General" : parameter.group) != group)
                continue;
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(parameter.label.c_str()).x);
        }
        labelWidth += ImGui::GetFontSize();
        bool properties = false;
        for (const auto& parameter : selected.parameters) {
            if (!matches(parameter)) continue;
            if ((parameter.group.empty() ? "General" : parameter.group) != group) continue;
            const auto found = snapshot.parameters.find(parameter.id);
            if (found == snapshot.parameters.end()) continue;
            const bool choiceGrid = parameter.kind == PreviewParameterKind::Choice && parameter.choiceButtons;
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
            if (InspectorParameterField(parameter, value, snapshot.pending.contains(parameter.id)))
                actions.parameter(parameter.id, value);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        if (properties) EndInspectorProperties();
        ImGui::PopID();
    }
    if (groups.empty() && (_search.front() != '\0' || _modifiedOnly))
        EditorEmptyState("No matching properties", "Change the search or turn off Modified only.");
    if (!selected.actions.empty() && InspectorSection("Actions"))
        InspectorActionGroup(selected.actions, snapshot.inspection.disabledActions, actions.action);
    ImGui::EndDisabled();
    if (!snapshot.inspection.values.empty() && InspectorSection("Live information")) {
        float labelWidth = 0;
        for (const auto& value : snapshot.inspection.values)
            labelWidth = std::max(labelWidth, ImGui::CalcTextSize(value.label.c_str()).x);
        if (BeginInspectorProperties("##live", labelWidth + ImGui::GetFontSize())) {
            for (const auto& value : snapshot.inspection.values)
                InspectorReadOnlyField(value);
            EndInspectorProperties();
        }
    }
    ImGui::PopID();
    EndInspectorStyle();
}
