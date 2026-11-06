#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <map>
#include <string>
#include "Editor/Internals/Session/QuickOpen.h"
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
    ImGui::PushStyleColor(ImGuiCol_Button, selected ? ToolColors().selected : ImVec4{0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? ToolColors().selected : ToolColors().hover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToolColors().pressed);
    const bool pressed = ImGui::Button("##filter", {size, size});
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    DrawCatalogFilterIcon(icon, {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f}, ToolUiScale(),
                          ImGui::GetColorU32(selected ? ToolColors().accent : ToolColors().muted));
    ToolTooltip(tooltip);
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
    ImGui::PopID();
    return pressed;
}

// An empty category shows as General, so the two are one group.
const std::string& CategoryLabel(const SceneDefinition& entry) {
    static const std::string General = "General";
    return entry.category.empty() ? General : entry.category;
}
} // namespace

void CatalogPanel::Entry(const SceneDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         EditorPreferences& preferences) {
    ImGui::PushID(entry.id.c_str());
    const bool favorite = preferences.IsFavorite(entry.id);
    const bool selected = snapshot.selected && snapshot.selected->id == entry.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    const float starInset = ToolMetrics::Space * ToolUiScale();
    const float starWidth = ImGui::GetFrameHeight();
    const float rowWidth = ImGui::GetContentRegionAvail().x;
    ImGui::BeginDisabled(!enabled);
    const bool clicked =
        ToolWrappedSelectable(entry.name.c_str(), selected, ImGuiSelectableFlags_None, starWidth + starInset);
    ToolTooltip(entry.description.c_str());
    if (selected && _reveal && !ImGui::IsItemVisible()) ImGui::SetScrollHereY(0.5f);
    if (enabled && clicked) {
        actions.select(entry.id);
        actions.command(EditorCommand::Open);
    }
    ImGui::EndDisabled();
    // The button remains submitted through mouse-down/release; only its icon hides when the row is idle.
    const auto rowStart = ImGui::GetItemRectMin();
    const auto selectableEnd = ImGui::GetItemRectMax();
    const auto rowEnd = ImVec2{rowStart.x + rowWidth, ImGui::GetItemRectMax().y};
    const bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(rowStart, rowEnd);
    if (selected || hovered)
        ImGui::GetWindowDrawList()->AddRectFilled(
            {selectableEnd.x, rowStart.y}, rowEnd,
            ImGui::GetColorU32(selected ? ImGuiCol_Header : ImGuiCol_HeaderHovered));
    const auto after = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(
        {rowEnd.x - starWidth - starInset, rowStart.y + (rowEnd.y - rowStart.y - starWidth) * 0.5f});
    ImGui::PushStyleColor(ImGuiCol_Text, favorite  ? ToolColors().accent
                                         : hovered ? ToolColors().muted
                                                   : ImVec4{0, 0, 0, 0});
    if (EditorIconButton("favorite", EditorGlyph::Star, favorite ? "Remove favorite" : "Add favorite"))
        preferences.ToggleFavorite(entry.id);
    ImGui::PopStyleColor();
    // Dummy clears ImGui's pending SetCursorPos state; starting one spacing above preserves `after`.
    ImGui::SetCursorScreenPos({after.x, after.y - ImGui::GetStyle().ItemSpacing.y});
    ImGui::Dummy({0, 0});
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
    EditorSearchInput("##search", "Search scenes", _search.data(), _search.size(), ImGui::GetContentRegionAvail().x,
                      _focusSearch);
    if (_view != 0) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", _view == 1 ? "Favorites" : "Recent");
        ContinueToolToolbar("Reset filters");
        if (ToolButton("Reset filters", {std::max(1.0f, ImGui::GetContentRegionAvail().x), 0})) {
            _search.fill('\0');
            _view = 0;
        }
        ToolTooltip("Clear the search and show all scenes.");
    }
    ImGui::PopStyleVar();
    std::vector<const SceneDefinition*> entries;
    const auto query = EditorSearchText(_search.data());
    std::size_t sceneCount = 0;
    std::size_t scopeCount = 0;
    for (const auto& entry : snapshot.catalog) {
        if (entry.asset) continue; // The Assets panel's
        ++sceneCount;
        if (_view == 1 && !preferences.IsFavorite(entry.id)) continue;
        if (_view == 2 && std::ranges::find(preferences.recent, entry.id) == preferences.recent.end()) continue;
        ++scopeCount;
        if (QuickOpenScore(entry.name, entry.description + " " + entry.category, query) < 0) continue;
        entries.push_back(&entry);
    }
    if (_view == 2) {
        std::ranges::sort(entries, [&](const auto* a, const auto* b) {
            return std::ranges::find(preferences.recent, a->id) < std::ranges::find(preferences.recent, b->id);
        });
    } else {
        // Read over the whole catalog, so a search does not reorder the categories.
        std::map<std::string, int> categoryOrders;
        for (const auto& entry : snapshot.catalog) {
            if (entry.asset) continue;
            const auto [order, added] = categoryOrders.try_emplace(CategoryLabel(entry), entry.categoryOrder);
            if (!added) order->second = std::min(order->second, entry.categoryOrder);
        }
        std::ranges::stable_sort(entries, [&](const auto* a, const auto* b) {
            const auto& labelA = CategoryLabel(*a);
            const auto& labelB = CategoryLabel(*b);
            const int orderA = categoryOrders.at(labelA);
            const int orderB = categoryOrders.at(labelB);
            if (orderA != orderB) return orderA < orderB;
            return labelA < labelB;
        });
    }
    const float scale = ToolUiScale();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {4 * scale, 2 * scale});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4 * scale, 1 * scale});
    if (ImGui::BeginChild("catalog-entries")) {
        std::map<std::string, int> counts;
        for (const auto* entry : entries)
            ++counts[CategoryLabel(*entry)];
        std::string category;
        bool expanded = true;
        bool first = true;
        for (const auto* entry : entries) {
            if (_view != 2 && (first || category != CategoryLabel(*entry))) {
                category = CategoryLabel(*entry);
                if (_reveal && snapshot.selected && CategoryLabel(*snapshot.selected) == category)
                    ImGui::GetStateStorage()->SetBool(ImGui::GetID(category.c_str()), true);
                expanded = ToolSectionHeader(category.c_str(), false, !query.empty(),
                                             std::to_string(counts[category]).c_str());
                first = false;
            }
            if (expanded) Entry(*entry, snapshot, actions, preferences);
        }
        _reveal = false;
        if (entries.empty()) {
            if (sceneCount == 0) {
                ToolEmptyState("No scenes available", "Check Output for details, or refresh the scene catalog.");
                ImGui::BeginDisabled(!snapshot.capabilities.Can(EditorCommand::Refresh));
                if (ToolButton("Build & refresh")) actions.command(EditorCommand::Refresh);
                ImGui::EndDisabled();
            } else {
                const char* title = scopeCount == 0 ? (_view == 1 ? "No favorites yet" : "No recent scenes")
                                    : _view == 1    ? "No matching favorite scenes"
                                    : _view == 2    ? "No matching recent scenes"
                                                    : "No matching scenes";
                const char* detail = scopeCount == 0 ? (_view == 1 ? "Star a scene to keep it here."
                                                                   : "Scenes you open will appear here.")
                                     : _view == 1    ? "Your search is limited to Favorites."
                                     : _view == 2    ? "Your search is limited to Recent."
                                                     : "Try a different name, description, or category.";
                ToolEmptyState(title, detail);
                if (!query.empty() && ToolButton("Clear search")) {
                    _search.fill('\0');
                    _focusSearch = true;
                }
                if (_view != 0) {
                    if (!query.empty()) ContinueToolToolbar("Reset filters");
                    if (ToolButton("Reset filters", {std::max(1.0f, ImGui::GetContentRegionAvail().x), 0})) {
                        _search.fill('\0');
                        _view = 0;
                    }
                    ToolTooltip("Clear the search and show all scenes.");
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
