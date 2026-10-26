#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorPlaybackControls.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"

bool ViewportPanel::Toolbar(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool maximized) {
    bool toggleMaximize = false;
    const auto& style = ImGui::GetStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, {style.FramePadding.x * 0.6f, style.FramePadding.y * 0.5f});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {style.ItemSpacing.x * 0.75f, style.ItemSpacing.y * 0.5f});
    // Only this row scrolls horizontally; the canvas keeps the full panel width.
    if (ImGui::BeginChild("preview-toolbar", {0, 0}, ImGuiChildFlags_AutoResizeY,
                          ImGuiWindowFlags_HorizontalScrollbar)) {
        DrawEditorPlaybackControls(snapshot, actions);
        ImGui::SameLine();
        const auto separator = ImGui::GetCursorScreenPos();
        const float height = ImGui::GetFrameHeight();
        ImGui::GetWindowDrawList()->AddLine({separator.x, separator.y + style.FramePadding.y},
                                            {separator.x, separator.y + height - style.FramePadding.y},
                                            ImGui::GetColorU32(ImGuiCol_Separator));
        ImGui::Dummy({1, height});
        constexpr std::array Modes{"Fit", "Focus content", "100%"};
        for (int i = 0; i < 3; ++i) {
            ImGui::SameLine();
            if (ImGui::RadioButton(Modes[i], _mode == i)) {
                _mode = i;
                _pan = {};
                _zoom = 1;
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        ImGui::SliderFloat("Zoom", &_zoom, 0.25f, 4.0f, "%.2fx");
        EditorTooltip("Middle-drag to pan; Ctrl+wheel to zoom. Product input uses the displayed image coordinates.");
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("View");
        toggleMaximize = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        ImGui::SameLine();
        const auto* expand = EditorGlyphText(maximized ? EditorGlyph::Restore : EditorGlyph::Expand);
        if (ImGui::Button(expand)) toggleMaximize = true;
        EditorTooltip(maximized ? "Restore workspace" : "Maximize preview (or double-click this heading)");
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    return toggleMaximize;
}

EditorViewportInteraction ViewportPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                              const EditorPreviewImage& image, bool metrics, bool maximized) {
    EditorViewportInteraction result;
    if (snapshot.selected && _preview != snapshot.selected->id) {
        _preview = snapshot.selected->id;
        _pan = {};
        _zoom = 1.0f;
    }
    result.toggleMaximize = Toolbar(snapshot, actions, maximized);
    if (metrics && snapshot.ready) {
        ImGui::TextWrapped("%dx%d | #%llu | %.1f FPS", image.width, image.height,
                           static_cast<unsigned long long>(image.sequence), snapshot.streamFps);
        ImGui::TextWrapped("Readback %.1f | publish %.1f | transfer %.1f ms", snapshot.captureMs, snapshot.publishMs,
                           image.transferMs);
    }
    ImGui::Separator();
    if (snapshot.ready && !snapshot.inspection.tables.empty()) {
        const float tableHeight = snapshot.inspection.showImage ? ImGui::GetContentRegionAvail().y * 0.65f : 0.0f;
        ImGui::BeginChild("native-content", {0, tableHeight}, ImGuiChildFlags_Borders);
        if (ImGui::BeginTabBar("data-views")) {
            for (const auto& table : snapshot.inspection.tables) {
                if (!ImGui::BeginTabItem(table.name.c_str())) continue;
                if (ImGui::BeginTable(table.name.c_str(), static_cast<int>(table.columns.size()),
                                      ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                                          ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) {
                    for (const auto& column : table.columns)
                        ImGui::TableSetupColumn(column.c_str());
                    ImGui::TableSetupScrollFreeze(0, 1);
                    ImGui::TableHeadersRow();
                    for (const auto& row : table.rows) {
                        ImGui::TableNextRow();
                        for (std::size_t c = 0; c < row.size(); ++c) {
                            ImGui::TableSetColumnIndex(static_cast<int>(c));
                            ImGui::TextUnformatted(row[c].c_str());
                        }
                    }
                    ImGui::EndTable();
                }
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
        ImGui::EndChild();
        if (!snapshot.inspection.showImage) {
            _focused = false;
            return result;
        }
    }
    const auto cursor = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const bool hasFrame = snapshot.ready && image.texture && image.generation == snapshot.generation;
    if (hasFrame && available.x > 1 && available.y > 1) {
        PreviewContentBounds bounds{0, 0, snapshot.canvas.x, snapshot.canvas.y};
        if (_mode == 1 && snapshot.inspection.focus) bounds = *snapshot.inspection.focus;
        bounds.x = std::clamp(bounds.x, 0, snapshot.canvas.x - 1);
        bounds.y = std::clamp(bounds.y, 0, snapshot.canvas.y - 1);
        bounds.width = std::clamp(bounds.width, 1, snapshot.canvas.x - bounds.x);
        bounds.height = std::clamp(bounds.height, 1, snapshot.canvas.y - bounds.y);
        const float scale = (_mode == 2 ? 1.0f
                                        : std::min(available.x / static_cast<float>(bounds.width),
                                                   available.y / static_cast<float>(bounds.height))) *
                            _zoom;
        ImGui::InvisibleButton("preview-image", available,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonMiddle);
        result.hovered = ImGui::IsItemHovered();
        result.contentActive = ImGui::IsItemActive();
        const auto& io = ImGui::GetIO();
        const bool navigating = result.hovered && (io.KeyCtrl || ImGui::IsMouseDown(ImGuiMouseButton_Middle));
        if (navigating) {
            if (io.KeyCtrl && io.MouseWheel != 0)
                _zoom = std::clamp(_zoom * (1.0f + io.MouseWheel * 0.1f), 0.25f, 4.0f);
            if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                _pan.x += io.MouseDelta.x;
                _pan.y += io.MouseDelta.y;
            }
        }
        const Float2 size{static_cast<float>(snapshot.canvas.x) * scale, static_cast<float>(snapshot.canvas.y) * scale};
        const Float2 position{
            cursor.x + available.x * 0.5f -
                (static_cast<float>(bounds.x) + static_cast<float>(bounds.width) * 0.5f) * scale + _pan.x,
            cursor.y + available.y * 0.5f -
                (static_cast<float>(bounds.y) + static_cast<float>(bounds.height) * 0.5f) * scale + _pan.y};
        result.viewport = {position, size, snapshot.canvas, {cursor.x, cursor.y}, {available.x, available.y}};
        auto* draw = ImGui::GetWindowDrawList();
        draw->PushClipRect(cursor, {cursor.x + available.x, cursor.y + available.y}, true);
        draw->AddImage(ImTextureID(image.texture), {position.x, position.y},
                       {position.x + size.x, position.y + size.y});
        draw->PopClipRect();
        if (navigating) result.hovered = false;
        if (ImGui::IsItemClicked()) _focused = true;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()) _focused = false;
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) _focused = false;
        result.focused = _focused;
    } else {
        _focused = false;
    }
    return result;
}
