#include "Editor/Ui/Panels/Inspector/InspectorPanel.h"
#include <imgui.h>
#include <algorithm>
#include "Editor/Ui/Widgets/InspectorWidgets.h"

void InspectorPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions) const {
    BeginInspectorStyle();
    if (!snapshot.selected) {
        ImGui::TextWrapped("Select a preview to inspect its controls.");
        EndInspectorStyle();
        return;
    }
    const auto& selected = *snapshot.selected;
    const auto hidden = [&](const std::string& id) {
        return id == "language" || std::ranges::find(snapshot.inspection.hiddenParameters, id) !=
                                       snapshot.inspection.hiddenParameters.end();
    };
    const auto disabled = [&](const std::string& id) {
        return std::ranges::find(snapshot.inspection.disabledParameters, id) !=
               snapshot.inspection.disabledParameters.end();
    };
    ImGui::PushID(selected.id.c_str());
    ImGui::PushTextWrapPos(0);
    ImGui::TextUnformatted(selected.description.c_str());
    ImGui::PopTextWrapPos();
    std::vector<std::string> groups;
    for (const auto& parameter : selected.parameters) {
        if (hidden(parameter.id)) continue;
        const auto group = parameter.group.empty() ? "General" : parameter.group;
        if (std::ranges::find(groups, group) == groups.end()) groups.push_back(group);
    }
    ImGui::BeginDisabled(!snapshot.ready);
    for (const auto& group : groups) {
        if (!InspectorSection(group.c_str()) || !BeginInspectorProperties(group.c_str())) continue;
        for (const auto& parameter : selected.parameters) {
            if (hidden(parameter.id)) continue;
            if ((parameter.group.empty() ? "General" : parameter.group) != group) continue;
            const auto found = snapshot.parameters.find(parameter.id);
            if (found == snapshot.parameters.end()) continue;
            ImGui::PushID(parameter.id.c_str());
            ImGui::BeginDisabled(disabled(parameter.id));
            auto value = found->second;
            if (InspectorParameterField(parameter, value, snapshot.pending.contains(parameter.id)))
                actions.parameter(parameter.id, value);
            ImGui::EndDisabled();
            ImGui::PopID();
        }
        EndInspectorProperties();
    }
    if (!selected.actions.empty() && InspectorSection("Actions"))
        InspectorActionGroup(selected.actions, snapshot.inspection.disabledActions, actions.action);
    ImGui::EndDisabled();
    if (!snapshot.inspection.values.empty() && InspectorSection("Live information") &&
        BeginInspectorProperties("##live")) {
        for (const auto& value : snapshot.inspection.values)
            InspectorReadOnlyField(value);
        EndInspectorProperties();
    }
    ImGui::PopID();
    EndInspectorStyle();
}
