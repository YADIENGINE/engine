#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Internals/Session/ViewportMark.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr double NoticeSeconds = 3.0; // A control notice stays this long, fading over the last NoticeFade
constexpr double NoticeFade = 0.75;
constexpr float MarkFill = 0.15f;   // Alpha of the marked area
constexpr float MarkOutline = 2.0f; // Pixels at 100% UI scale
// The last change made through the control endpoint, over the top-left corner of the viewport area.
void DrawNotice(const EditorSnapshot& snapshot, ImVec2 cursor) {
    if (snapshot.notice.empty() || snapshot.noticeAge >= NoticeSeconds) return;
    const float alpha = static_cast<float>(std::clamp((NoticeSeconds - snapshot.noticeAge) / NoticeFade, 0.0, 1.0));
    const auto text = "Control: " + snapshot.notice;
    const auto& style = ImGui::GetStyle();
    const ImVec2 size = ImGui::CalcTextSize(text.c_str());
    const ImVec2 at{cursor.x + style.WindowPadding.x, cursor.y + style.WindowPadding.y};
    const ImVec2 end{at.x + size.x + style.FramePadding.x * 2.0f, at.y + size.y + style.FramePadding.y * 2.0f};
    auto* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(at, end, ImGui::GetColorU32(ImGuiCol_PopupBg, alpha), style.FrameRounding);
    draw->AddRect(at, end, ImGui::GetColorU32(ImGuiCol_Border, alpha), style.FrameRounding);
    draw->AddText({at.x + style.FramePadding.x, at.y + style.FramePadding.y}, ImGui::GetColorU32(ImGuiCol_Text, alpha),
                  text.c_str());
}
} // namespace

bool ViewportPanel::Toolbar(bool maximized) {
    bool toggleMaximize = false;
    const auto& style = ImGui::GetStyle();
    constexpr std::array Modes{"Fit", "Focus content", "100%"};
    if (ImGui::Button("View")) ImGui::OpenPopup("viewport-view");
    if (ImGui::BeginPopup("viewport-view")) {
        for (int i = 0; i < 3; ++i) {
            if (ImGui::MenuItem(Modes[static_cast<std::size_t>(i)], nullptr, _mode == i)) {
                _mode = i;
                _pan = {};
                _zoom = 1;
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        ImGui::SliderFloat("Zoom", &_zoom, 0.25f, 4.0f, "%.2fx");
        ToolTooltip("Middle-drag to pan; Ctrl+wheel to zoom. Product input uses the displayed image coordinates.");
        if (ImGui::MenuItem("Reset pan and zoom")) {
            _pan = {};
            _zoom = 1;
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    const float maximizeWidth = ImGui::GetFrameHeight();
    if (ImGui::GetContentRegionAvail().x > ImGui::GetFontSize() * 13.0f + maximizeWidth) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s / %.0f%%", Modes[static_cast<std::size_t>(_mode)], _zoom * 100.0f);
        toggleMaximize = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        ToolTooltip("Double-click to maximize or restore the viewport.");
        ImGui::SameLine();
    }
    const float gap = ImGui::GetContentRegionAvail().x - maximizeWidth;
    if (gap > style.ItemSpacing.x) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + gap);
    if (EditorIconButton("maximize-viewport", maximized ? EditorGlyph::Restore : EditorGlyph::Expand,
                         maximized ? "Restore workspace" : "Maximize viewport"))
        toggleMaximize = true;
    return toggleMaximize;
}

EditorViewportInteraction ViewportPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                              const ViewportImage& image, bool metrics, bool maximized) {
    EditorViewportInteraction result;
    if (snapshot.selected && _scene != snapshot.selected->id) {
        _scene = snapshot.selected->id;
        _pan = {};
        _zoom = 1.0f;
    }
    result.toggleMaximize = Toolbar(maximized);
    if (metrics && snapshot.ready) {
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), 0);
        ImGui::TextWrapped("%dx%d | #%llu | %.1f Stream FPS", image.width, image.height,
                           static_cast<unsigned long long>(image.sequence), snapshot.streamFps);
        ImGui::TextWrapped("Readback %.1f | publish %.1f | transfer %.1f ms", snapshot.captureMs, snapshot.publishMs,
                           image.transferMs);
        ImGui::PopFont();
    }
    ImGui::Separator();
    if (snapshot.ready && !snapshot.inspection.tables.empty()) {
        const float tableHeight = snapshot.inspection.showImage ? ImGui::GetContentRegionAvail().y * 0.65f : 0.0f;
        ImGui::BeginChild("native-content", {0, tableHeight}, ImGuiChildFlags_Borders);
        if (ImGui::BeginTabBar("data-views")) {
            for (const auto& table : snapshot.inspection.tables) {
                if (!ImGui::BeginTabItem(table.name.c_str())) continue;
                if (ImGui::BeginTable(table.name.c_str(), static_cast<int>(table.columns.size()),
                                      ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
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
        SceneContentBounds bounds{0, 0, snapshot.canvas.x, snapshot.canvas.y};
        if (_mode == 1 && snapshot.inspection.focus) bounds = *snapshot.inspection.focus;
        bounds.x = std::clamp(bounds.x, 0, snapshot.canvas.x - 1);
        bounds.y = std::clamp(bounds.y, 0, snapshot.canvas.y - 1);
        bounds.width = std::clamp(bounds.width, 1, snapshot.canvas.x - bounds.x);
        bounds.height = std::clamp(bounds.height, 1, snapshot.canvas.y - bounds.y);
        const float scale = (_mode == 2 ? 1.0f
                                        : std::min(available.x / static_cast<float>(bounds.width),
                                                   available.y / static_cast<float>(bounds.height))) *
                            _zoom;
        ImGui::InvisibleButton("viewport-image", available,
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
        // Alt+drag (or click) marks a region of the scene; it pauses so the frame holds still. The toolbar's Mark
        // toggle works over the whole window (EditorWorkspace::MarkOverlay).
        const bool gesture = io.KeyAlt && !navigating;
        const Float2 mouse{io.MousePos.x, io.MousePos.y};
        if (gesture && result.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _markStart = mouse;
            if (snapshot.ready && !snapshot.paused && snapshot.capabilities.Can(EditorCommand::Pause))
                actions.command(EditorCommand::Pause);
        }
        if (_markStart) {
            const ImVec2 from{std::min(_markStart->x, mouse.x), std::min(_markStart->y, mouse.y)};
            const ImVec2 to{std::max(_markStart->x, mouse.x), std::max(_markStart->y, mouse.y)};
            draw->AddRectFilled(from, to, ImGui::GetColorU32(ImGuiCol_DragDropTarget, MarkFill));
            draw->AddRect(from, to, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 0.0f, 0, MarkOutline * ToolUiScale());
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                result.mark = MarkRegion(result.viewport, *_markStart, mouse);
                _markStart.reset();
            }
        }
        result.marking = gesture || _markStart.has_value();
        if (navigating) result.hovered = false;
        if (ImGui::IsItemClicked()) _focused = true;
        if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsItemHovered()) _focused = false;
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows)) _focused = false;
        result.focused = _focused;
    } else {
        _focused = false;
        _markStart.reset();
        if (!snapshot.selected) {
            ToolEmptyState("Choose a scene", "Select an item from the Catalog to start a scene.");
        } else if (snapshot.phase == EditorPhase::Error) {
            ToolEmptyState("Scene unavailable", "See Output for details, then retry or build and refresh.");
        } else if (snapshot.phase == EditorPhase::Stopped) {
            ToolEmptyState("Scene stopped", "Start the selected scene when you are ready.");
        } else {
            ToolEmptyState(snapshot.ready ? "Waiting for a frame" : EditorPhaseLabel(snapshot.phase),
                           snapshot.status.c_str());
        }
        if (snapshot.selected && snapshot.capabilities.Can(EditorCommand::Open)) {
            if (ImGui::Button(snapshot.phase == EditorPhase::Error ? "Retry scene" : "Start scene"))
                actions.command(EditorCommand::Open);
        } else if (snapshot.capabilities.Can(EditorCommand::Cancel)) {
            if (ImGui::Button("Cancel")) actions.command(EditorCommand::Cancel);
        }
    }
    DrawNotice(snapshot, cursor);
    return result;
}
