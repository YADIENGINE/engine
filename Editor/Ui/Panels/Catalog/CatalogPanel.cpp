#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include <imgui.h>
#include <algorithm>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

void CatalogPanel::Entry(const PreviewDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         EditorPreferences& preferences) {
    ImGui::PushID(entry.id.c_str());
    const bool favorite = preferences.IsFavorite(entry.id);
    ImGui::PushStyleColor(ImGuiCol_Text, favorite ? EditorToneColor(EditorTone::Warning)
                                                  : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (EditorIconButton("favorite", EditorGlyph::Star, favorite ? "Remove favorite" : "Add favorite", favorite))
        preferences.ToggleFavorite(entry.id);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    const bool selected = snapshot.selected && snapshot.selected->id == entry.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    ImGui::BeginDisabled(!enabled);
    const bool clicked = EditorWrappedSelectable(entry.name.c_str(), selected, ImGuiSelectableFlags_None);
    EditorTooltip(entry.description.c_str());
    if (enabled && clicked) {
        actions.select(entry.id);
        actions.command(EditorCommand::Open);
    }
    ImGui::EndDisabled();
    ImGui::PopID();
}
void CatalogPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                        EditorPreferences& preferences) {
    ImGui::SetNextItemWidth(-1);
    if (_focusSearch) {
        ImGui::SetKeyboardFocusHere();
        _focusSearch = false;
    }
    ImGui::InputTextWithHint("##search", "Search previews (Ctrl+P)", _search.data(), _search.size());
    constexpr std::array Labels{"All", "Favorites", "Recent"};
    for (int i = 0; i < static_cast<int>(Labels.size()); ++i) {
        if (i) ContinueEditorToolbar(Labels[static_cast<std::size_t>(i)]);
        if (EditorSegment(Labels[static_cast<std::size_t>(i)], _view == i)) _view = i;
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
        std::ranges::stable_sort(entries, [](const auto* a, const auto* b) {
            if (a->categoryOrder != b->categoryOrder) return a->categoryOrder < b->categoryOrder;
            return a->category < b->category;
        });
    }
    const float scale = EditorUiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * scale, 2 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4 * scale, 2 * scale});
    if (ImGui::BeginChild("catalog-entries")) {
        std::string category;
        bool expanded = true;
        bool first = true;
        for (const auto* entry : entries) {
            if (_view != 2 && (first || category != entry->category)) {
                category = entry->category;
                expanded = EditorSectionHeader(category.empty() ? "General" : category.c_str(), false, !query.empty());
                first = false;
            }
            if (expanded) Entry(*entry, snapshot, actions, preferences);
        }
        if (entries.empty()) {
            if (snapshot.catalog.empty()) {
                EditorEmptyState("No previews available", "Check Output for details, or refresh the preview catalog.");
                ImGui::BeginDisabled(!snapshot.capabilities.Can(EditorCommand::Refresh));
                if (ImGui::Button("Build & refresh")) actions.command(EditorCommand::Refresh);
                ImGui::EndDisabled();
            } else {
                const char* detail = !query.empty() ? "Try a different name, description, or category."
                                     : _view == 1   ? "Star a preview to keep it here."
                                                    : "Previews you open will appear here.";
                EditorEmptyState(!query.empty() ? "No matching previews"
                                 : _view == 1   ? "No favorites yet"
                                                : "No recent previews",
                                 detail);
                if (ImGui::Button(!query.empty() ? "Clear search" : "Show all previews")) {
                    _search.fill('\0');
                    if (query.empty()) _view = 0;
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
