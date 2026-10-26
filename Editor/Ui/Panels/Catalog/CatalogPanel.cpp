#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include <imgui.h>
#include <algorithm>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

void CatalogPanel::Entry(const PreviewDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         EditorPreferences& preferences) {
    ImGui::PushID(entry.id.c_str());
    const bool favorite = preferences.IsFavorite(entry.id);
    ImGui::PushStyleColor(ImGuiCol_Text, favorite ? EditorPhaseColor(EditorPhase::Paused)
                                                  : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (ImGui::SmallButton(EditorGlyphText(EditorGlyph::Star))) preferences.ToggleFavorite(entry.id);
    ImGui::PopStyleColor();
    EditorTooltip(favorite ? "Remove favorite" : "Add favorite");
    ImGui::SameLine();
    const bool selected = snapshot.selected && snapshot.selected->id == entry.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    ImGui::BeginDisabled(!enabled);
    const bool clicked = EditorWrappedSelectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick);
    const bool open = (clicked && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ||
                      (ImGui::IsItemFocused() && (ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                                  ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)));
    EditorTooltip(entry.description.c_str());
    if (enabled && (clicked || open)) {
        actions.select(entry.id);
        if (open) actions.command(EditorCommand::Open);
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}
void CatalogPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                        EditorPreferences& preferences) {
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search previews...", _search.data(), _search.size());
    if (ImGui::BeginTabBar("views")) {
        constexpr std::array Labels{"All", "Favorites", "Recent"};
        for (int i = 0; i < static_cast<int>(Labels.size()); ++i) {
            if (ImGui::BeginTabItem(Labels[static_cast<std::size_t>(i)])) {
                _view = i;
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    std::vector<const PreviewDefinition*> entries;
    const auto query = EditorSearchText(_search.data());
    for (const auto& entry : snapshot.catalog) {
        if (_view == 1 && !preferences.IsFavorite(entry.id)) continue;
        if (_view == 2 && std::ranges::find(preferences.recent, entry.id) == preferences.recent.end()) continue;
        if (!query.empty() &&
            EditorSearchText(entry.name + " " + entry.description + " " + entry.category).find(query) ==
                std::string::npos)
            continue;
        entries.push_back(&entry);
    }
    if (_view == 2) {
        std::ranges::sort(entries, [&](const auto* a, const auto* b) {
            return std::ranges::find(preferences.recent, a->id) < std::ranges::find(preferences.recent, b->id);
        });
    } else {
        std::ranges::stable_sort(entries, [](const auto* a, const auto* b) { return a->category < b->category; });
    }
    std::string category;
    bool expanded = true;
    bool first = true;
    for (const auto* entry : entries) {
        if (_view != 2 && (first || category != entry->category)) {
            category = entry->category;
            expanded = ImGui::CollapsingHeader(category.empty() ? "General" : category.c_str(),
                                               ImGuiTreeNodeFlags_DefaultOpen);
            first = false;
        }
        if (expanded) Entry(*entry, snapshot, actions, preferences);
    }
    if (entries.empty())
        ImGui::TextWrapped(snapshot.catalog.empty() ? "No catalog available. Check Output for details."
                                                    : "No matching previews.");
}
