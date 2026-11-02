#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float FilterIconSize = 14;
constexpr float FilterStrokeWidth = 1.5f;
enum class CatalogFilterIcon { Star, Clock };

void DrawCatalogFilterIcon(CatalogFilterIcon icon, ImVec2 center, float scale, ImU32 color) {
    auto* draw = ImGui::GetWindowDrawList();
    const float size = FilterIconSize * scale;
    const float stroke = FilterStrokeWidth * scale;
    if (icon == CatalogFilterIcon::Star) {
        // Center the visible outline, independently of font bearings and baseline.
        static constexpr std::array Points{ImVec2{0, -0.5f},      ImVec2{0.15f, -0.17f}, ImVec2{0.5f, -0.12f},
                                           ImVec2{0.24f, 0.14f},  ImVec2{0.31f, 0.5f},   ImVec2{0, 0.31f},
                                           ImVec2{-0.31f, 0.5f},  ImVec2{-0.24f, 0.14f}, ImVec2{-0.5f, -0.12f},
                                           ImVec2{-0.15f, -0.17f}};
        for (const auto& point : Points)
            draw->PathLineTo({center.x + point.x * size, center.y + point.y * size});
        draw->PathStroke(color, ImDrawFlags_Closed, stroke);
    } else {
        draw->AddCircle(center, size * 0.47f, color, 32, stroke);
        draw->PathLineTo({center.x, center.y - size * 0.25f});
        draw->PathLineTo(center);
        draw->PathLineTo({center.x + size * 0.2f, center.y + size * 0.12f});
        draw->PathStroke(color, ImDrawFlags_None, stroke);
    }
}

bool CatalogFilterButton(const char* id, CatalogFilterIcon icon, const char* tooltip, bool selected) {
    const float size = ImGui::GetFrameHeight();
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ToolPalette::Selected : ImVec4{0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? ToolPalette::Selected : ToolPalette::Hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolPalette::Pressed);
    const bool pressed = ImGui::Button("##filter", {size, size});
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    DrawCatalogFilterIcon(icon, {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f}, ToolUiScale(),
                          ImGui::GetColorU32(selected ? ToolPalette::Accent : ToolPalette::Muted));
    ToolTooltip(tooltip);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return pressed;
}
} // namespace

void CatalogPanel::Entry(const SceneDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         EditorPreferences& preferences) {
    ImGui::PushID(entry.id.c_str());
    const bool favorite = preferences.IsFavorite(entry.id);
    const bool selected = snapshot.selected && snapshot.selected->id == entry.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    const float starWidth = ImGui::GetFrameHeight();
    ImGui::BeginDisabled(!enabled);
    const bool clicked =
        ToolWrappedSelectable(entry.name.c_str(), selected, ImGuiSelectableFlags_AllowOverlap, starWidth);
    ToolTooltip(entry.description.c_str());
    if (selected && _reveal && !ImGui::IsItemVisible()) ImGui::SetScrollHereY(0.5f);
    if (enabled && clicked) {
        actions.select(entry.id);
        actions.command(EditorCommand::Open);
    }
    ImGui::EndDisabled();
    // The star stays out of the way until the row is hovered; a favorite always shows it.
    const auto rowStart = ImGui::GetItemRectMin();
    const auto rowEnd = ImGui::GetItemRectMax();
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rowStart, rowEnd);
    if (favorite || hovered) {
        const auto after = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos({rowEnd.x - starWidth, rowStart.y + (rowEnd.y - rowStart.y - starWidth) * 0.5f});
        ImGui::PushStyleColor(ImGuiCol_Text, favorite ? ToolPalette::Accent : ToolPalette::Muted);
        if (EditorIconButton("favorite", EditorGlyph::Star, favorite ? "Remove favorite" : "Add favorite"))
            preferences.ToggleFavorite(entry.id);
        ImGui::PopStyleColor();
        // Dummy clears ImGui's pending SetCursorPos state; starting one spacing above preserves `after`.
        ImGui::SetCursorScreenPos({after.x, after.y - ImGui::GetStyle().ItemSpacing.y});
        ImGui::Dummy({0, 0});
    }
    ImGui::PopID();
}
void CatalogPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                        EditorPreferences& preferences) {
    if (const auto scene = snapshot.selected ? snapshot.selected->id : std::string(); scene != _revealedScene) {
        _revealedScene = scene;
        _reveal = !scene.empty();
    }
    const float gap = ToolMetrics::Space * ToolUiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {gap, ImGui::GetStyle().ItemSpacing.y});
    if (CatalogFilterButton("favorites-filter", CatalogFilterIcon::Star,
                            _view == 1 ? "Favorites on. Click to show all scenes." : "Show favorite scenes.",
                            _view == 1))
        _view = _view == 1 ? 0 : 1;
    ImGui::SameLine();
    if (CatalogFilterButton("recent-filter", CatalogFilterIcon::Clock,
                            _view == 2 ? "Recent on. Click to show all scenes." : "Show recent scenes.", _view == 2))
        _view = _view == 2 ? 0 : 2;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(std::max(1.0f, ImGui::GetContentRegionAvail().x));
    ImGui::InputTextWithHint("##search", "Search scenes", _search.data(), _search.size());
    ImGui::PopStyleVar();
    std::vector<const SceneDefinition*> entries;
    const auto query = EditorSearchText(_search.data());
    for (const auto& entry : snapshot.catalog) {
        if (entry.asset) continue; // The Assets panel's
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
    const float scale = ToolUiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * scale, 2 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4 * scale, 1 * scale});
    if (ImGui::BeginChild("catalog-entries")) {
        std::map<std::string, int> counts;
        for (const auto* entry : entries)
            ++counts[entry->category];
        std::string category;
        bool expanded = true;
        bool first = true;
        for (const auto* entry : entries) {
            if (_view != 2 && (first || category != entry->category)) {
                category = entry->category;
                const char* name = category.empty() ? "General" : category.c_str();
                if (_reveal && snapshot.selected && snapshot.selected->category == category)
                    ImGui::GetStateStorage()->SetBool(ImGui::GetID(name), true);
                expanded = ToolSectionHeader(name, false, !query.empty(), std::to_string(counts[category]).c_str());
                first = false;
            }
            if (expanded) Entry(*entry, snapshot, actions, preferences);
        }
        _reveal = false;
        if (entries.empty()) {
            if (snapshot.catalog.empty()) {
                ToolEmptyState("No scenes available", "Check Output for details, or refresh the scene catalog.");
                ImGui::BeginDisabled(!snapshot.capabilities.Can(EditorCommand::Refresh));
                if (ToolButton("Build & refresh")) actions.command(EditorCommand::Refresh);
                ImGui::EndDisabled();
            } else {
                const char* detail = !query.empty() ? "Try a different name, description, or category."
                                     : _view == 1   ? "Star a scene to keep it here."
                                                    : "Scenes you open will appear here.";
                ToolEmptyState(!query.empty() ? "No matching scenes"
                               : _view == 1   ? "No favorites yet"
                                              : "No recent scenes",
                               detail);
                if (ToolButton(!query.empty() ? "Clear search" : "Show all scenes")) {
                    _search.fill('\0');
                    if (query.empty()) _view = 0;
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
