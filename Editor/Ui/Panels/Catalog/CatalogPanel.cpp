#include "Editor/Ui/Panels/Catalog/CatalogPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr float FilterButtonSize = 36;
constexpr float FilterIconSize = 16;
constexpr float FilterCornerRadius = 8;
constexpr float FilterStrokeWidth = 1.6f;
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
    const float scale = ToolUiScale();
    const float size = FilterButtonSize * scale;
    const auto accent = ToolToneColor(ToolTone::Accent);
    auto background = selected ? accent : ToolPalette::Control;
    auto hovered = selected ? accent : ToolPalette::Hover;
    auto pressedColor = selected ? accent : ToolPalette::Pressed;
    auto border = selected ? accent : ToolPalette::Border;
    if (selected) {
        background.w = 0.22f;
        hovered.w = 0.30f;
        pressedColor.w = 0.38f;
    } else {
        border.w = 0.45f;
    }
    ImGui::PushID(id);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, FilterCornerRadius * scale);
    ImGui::PushStyleColor(ImGuiCol_Button, background);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, pressedColor);
    ImGui::PushStyleColor(ImGuiCol_Border, border);
    const bool pressed = ImGui::Button("##filter", {size, size});
    const auto start = ImGui::GetItemRectMin();
    const auto end = ImGui::GetItemRectMax();
    DrawCatalogFilterIcon(icon, {(start.x + end.x) * 0.5f, (start.y + end.y) * 0.5f}, scale,
                          ImGui::GetColorU32(selected ? accent : ToolPalette::Secondary));
    ToolTooltip(tooltip);
    ImGui::PopStyleColor(4);
    ImGui::PopStyleVar(2);
    ImGui::PopID();
    return pressed;
}
} // namespace

void CatalogPanel::Entry(const SceneDefinition& entry, const EditorSnapshot& snapshot, const EditorUiActions& actions,
                         EditorPreferences& preferences) {
    ImGui::PushID(entry.id.c_str());
    const bool favorite = preferences.IsFavorite(entry.id);
    ImGui::PushStyleColor(ImGuiCol_Text, favorite ? ToolToneColor(ToolTone::Warning)
                                                  : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    if (EditorIconButton("favorite", EditorGlyph::Star, favorite ? "Remove favorite" : "Add favorite", favorite))
        preferences.ToggleFavorite(entry.id);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    const bool selected = snapshot.selected && snapshot.selected->id == entry.id;
    const bool enabled = !snapshot.busy || (selected && snapshot.capabilities.Can(EditorCommand::Open));
    ImGui::BeginDisabled(!enabled);
    const bool clicked = ToolWrappedSelectable(entry.name.c_str(), selected, ImGuiSelectableFlags_None);
    ToolTooltip(entry.description.c_str());
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
    ImGui::InputTextWithHint("##search", "Search scenes (Ctrl+P)", _search.data(), _search.size());
    if (CatalogFilterButton("favorites-filter", CatalogFilterIcon::Star,
                            _view == 1 ? "Favorites on. Click to show all scenes." : "Show favorite scenes.",
                            _view == 1))
        _view = _view == 1 ? 0 : 1;
    ContinueToolToolbar(FilterButtonSize * ToolUiScale());
    if (CatalogFilterButton("recent-filter", CatalogFilterIcon::Clock,
                            _view == 2 ? "Recent on. Click to show all scenes." : "Show recent scenes.", _view == 2))
        _view = _view == 2 ? 0 : 2;
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
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {4 * scale, 2 * scale});
    if (ImGui::BeginChild("catalog-entries")) {
        std::string category;
        bool expanded = true;
        bool first = true;
        for (const auto* entry : entries) {
            if (_view != 2 && (first || category != entry->category)) {
                category = entry->category;
                expanded = ToolSectionHeader(category.empty() ? "General" : category.c_str(), false, !query.empty());
                first = false;
            }
            if (expanded) Entry(*entry, snapshot, actions, preferences);
        }
        if (entries.empty()) {
            if (snapshot.catalog.empty()) {
                ToolEmptyState("No scenes available", "Check Output for details, or refresh the scene catalog.");
                ImGui::BeginDisabled(!snapshot.capabilities.Can(EditorCommand::Refresh));
                if (ImGui::Button("Build & refresh")) actions.command(EditorCommand::Refresh);
                ImGui::EndDisabled();
            } else {
                const char* detail = !query.empty() ? "Try a different name, description, or category."
                                     : _view == 1   ? "Star a scene to keep it here."
                                                    : "Scenes you open will appear here.";
                ToolEmptyState(!query.empty() ? "No matching scenes"
                               : _view == 1   ? "No favorites yet"
                                              : "No recent scenes",
                               detail);
                if (ImGui::Button(!query.empty() ? "Clear search" : "Show all scenes")) {
                    _search.fill('\0');
                    if (query.empty()) _view = 0;
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}
