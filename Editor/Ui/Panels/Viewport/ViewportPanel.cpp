#include "Editor/Ui/Panels/Viewport/ViewportPanel.h"
#include <imgui.h>
#include <algorithm>
#include <array>
#include <string>
#include "Editor/Internals/Session/ViewportMark.h"
#include "Editor/Ui/Style/EditorTheme.h"
#include "Editor/Ui/Widgets/EditorWidgets.h"
#include "ToolUi/ToolPixel.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

namespace {
constexpr double NoticeSeconds = 3.0; // A control notice stays this long, fading over the last NoticeFade
constexpr double NoticeFade = 0.75;
constexpr float MarkFill = 0.15f;   // Alpha of the marked area
constexpr float MarkOutline = 2.0f; // Pixels at 100% UI scale
constexpr float EmptyMarkCell = 6.0f;
constexpr float EmptyGap = 16.0f;
constexpr float RecentRowWidth = 320.0f;
constexpr std::size_t RecentRows = 5;
constexpr std::array ViewModes{"Fit", "Focus content", "Actual size"};
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
void CenterNext(float width, float areaWidth, float originX) {
    ImGui::SetCursorPosX(originX + std::max(0.0f, (areaWidth - width) * 0.5f));
}
struct EmptyViewportChoice {
    bool button = false;
    const SceneDefinition* recent = nullptr;
};
// One recently opened scene: its name, and its category at the row's far end.
bool RecentRow(const SceneDefinition& scene, float width) {
    ImGui::PushID(scene.id.c_str());
    const auto start = ImGui::GetCursorScreenPos();
    const bool pressed = ImGui::Selectable("##recent", false, 0, {width, ImGui::GetTextLineHeight()});
    ToolTooltip(scene.description.c_str());
    auto* draw = ImGui::GetWindowDrawList();
    const float categoryWidth = ImGui::CalcTextSize(scene.category.c_str()).x;
    const float inset = ToolMetrics::Space * ToolUiScale();
    const auto name = ToolClipText(scene.name.c_str(), std::max(1.0f, width - categoryWidth - inset * 4));
    draw->AddText({start.x + inset, start.y}, ImGui::GetColorU32(ToolColors().text), name.c_str());
    draw->AddText({start.x + width - categoryWidth - inset, start.y}, ImGui::GetColorU32(ToolColors().muted),
                  scene.category.c_str());
    ImGui::PopID();
    return pressed;
}
// The viewport with no frame to show: the mark and what to do next, then as room allows the scenes opened last and,
// with no scene yet, the shortcuts that get there.
EmptyViewportChoice DrawEmptyViewport(const char* title, const char* detail, const char* button, bool buttonStarts,
                                      const std::vector<const SceneDefinition*>& recent, bool hints, ImVec2 area) {
    struct Hint {
        const char* keys;
        const char* action;
    };
    static constexpr std::array Hints{Hint{"Ctrl+P", "Find anything"}, Hint{"Ctrl+Shift+P", "All commands"},
                                      Hint{"Ctrl+J", "Bottom panel"}, Hint{"F5", "Run or stop"}};
    const float scale = ToolUiScale();
    const auto& style = ImGui::GetStyle();
    const float cell = ToolPixelCell(EmptyMarkCell);
    const float titleHeight = ToolMetrics::HeadingSize * scale * 1.3f;
    const float rowHeight = ImGui::GetTextLineHeight() + ToolMetrics::Space * scale;
    const float base = ToolPixelTextSize("Y", cell).y + EmptyGap * scale + titleHeight + ImGui::GetTextLineHeight() +
                       (button ? ImGui::GetFrameHeight() + EmptyGap * scale : 0.0f);
    const float recentHeight =
        recent.empty() ? 0.0f : EmptyGap * 2 * scale + rowHeight * static_cast<float>(recent.size() + 1);
    const float hintsHeight = hints ? EmptyGap * 2 * scale + rowHeight * Hints.size() : 0.0f;
    // A short viewport gives up the shortcuts first, then the recent scenes.
    const bool showRecent = base + recentHeight <= area.y;
    const bool showHints = hints && base + (showRecent ? recentHeight : 0.0f) + hintsHeight <= area.y;
    const float block = base + (showRecent ? recentHeight : 0.0f) + (showHints ? hintsHeight : 0.0f);
    const float originX = ImGui::GetCursorPosX();
    const float width = std::max(1.0f, area.x);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(0.0f, (area.y - block) * 0.5f));
    CenterNext(ToolPixelTextSize("Y", cell).x, width, originX);
    ToolPixelText("Y", EmptyMarkCell, ToolColors().accent);
    ImGui::Dummy({0, EmptyGap * scale - style.ItemSpacing.y});
    ImGui::PushFont(ToolFont(ToolFontRole::Heading), ToolMetrics::HeadingSize);
    const auto titleText = ToolClipText(title, width);
    CenterNext(ImGui::CalcTextSize(titleText.c_str()).x, width, originX);
    ImGui::TextUnformatted(titleText.c_str());
    ImGui::PopFont();
    const auto detailText = ToolClipText(detail, width);
    CenterNext(ImGui::CalcTextSize(detailText.c_str()).x, width, originX);
    ImGui::PushStyleColor(ImGuiCol_Text, ToolColors().secondary);
    ImGui::TextUnformatted(detailText.c_str());
    ImGui::PopStyleColor();
    if (*detail && detailText != detail) ToolTooltip(detail);
    EmptyViewportChoice choice;
    if (button) {
        ImGui::Dummy({0, EmptyGap * scale - style.ItemSpacing.y * 2});
        CenterNext(ImGui::CalcTextSize(button).x + style.FramePadding.x * 2, width, originX);
        if (buttonStarts) PushEditorGoButton();
        choice.button = ToolButton(button);
        if (buttonStarts) PopEditorButton();
    }
    if (showRecent && !recent.empty()) {
        const float listWidth = std::min(RecentRowWidth * scale, width);
        ImGui::Dummy({0, EmptyGap * 2 * scale - style.ItemSpacing.y * 2});
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, ToolMetrics::Space * scale});
        CenterNext(listWidth, width, originX);
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ToolMetrics::Space * scale);
        ImGui::TextColored(ToolColors().muted, "Recent");
        for (const auto* scene : recent) {
            CenterNext(listWidth, width, originX);
            if (RecentRow(*scene, listWidth)) choice.recent = scene;
        }
        ImGui::PopStyleVar();
    }
    if (showHints) {
        ImGui::Dummy({0, EmptyGap * 2 * scale - style.ItemSpacing.y * 2});
        ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
        const float keyColumn = ImGui::CalcTextSize("Ctrl+Shift+P").x + ToolMetrics::SectionGap * scale;
        ImGui::PopFont();
        const float actionColumn = ImGui::CalcTextSize("Find anything").x;
        const float start = originX + std::max(0.0f, (width - keyColumn - actionColumn) * 0.5f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, ToolMetrics::Space * scale});
        for (const auto& hint : Hints) {
            ImGui::SetCursorPosX(start);
            ImGui::PushFont(ToolFont(ToolFontRole::Technical), ToolMetrics::CaptionSize);
            ImGui::TextColored(ToolColors().muted, "%s", hint.keys);
            ImGui::PopFont();
            ImGui::SameLine(start + keyColumn);
            ImGui::TextColored(ToolColors().secondary, "%s", hint.action);
        }
        ImGui::PopStyleVar();
    }
    return choice;
}
} // namespace

bool ViewportPanel::Toolbar(bool maximized) {
    bool toggleMaximize = false;
    // The real scale of the image: the mode's own scale (Fit and Focus content depend on the panel) times the zoom.
    const std::string mode = ViewModes[static_cast<std::size_t>(_mode)];
    const auto view = _shownScale > 0
                          ? mode + "  \xC2\xB7  " + std::to_string(static_cast<int>(_shownScale * 100.0f + 0.5f)) + "%"
                          : mode;
    const auto& style = ImGui::GetStyle();
    // Sized for the widest percentage, so resizing the panel does not make the chevron wander.
    ImGui::SetNextItemWidth(ImGui::CalcTextSize((mode + "  \xC2\xB7  400%").c_str()).x + style.FramePadding.x * 2 +
                            ImGui::GetFontSize() * 1.5f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_Text, ToolColors().secondary);
    const bool open = ToolBeginCombo("##viewport-view", view.c_str());
    ImGui::PopStyleColor(2);
    if (open) {
        for (int i = 0; i < static_cast<int>(ViewModes.size()); ++i) {
            if (ImGui::MenuItem(ViewModes[static_cast<std::size_t>(i)], nullptr, _mode == i)) {
                _mode = i;
                _pan = {};
                _zoom = 1;
            }
        }
        ImGui::Separator();
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        (void)ToolSlider("Zoom", _zoom, 0.25f, 4.0f, "%.2fx");
        ToolTooltip("Multiplies the mode's scale. Middle-drag to pan; Ctrl+wheel to zoom. Product input uses the "
                    "displayed image coordinates.");
        if (ImGui::MenuItem("Reset pan and zoom")) {
            _pan = {};
            _zoom = 1;
        }
        ImGui::EndCombo();
    } else {
        ToolTooltip("View mode and scale. Middle-drag to pan; Ctrl+wheel to zoom.");
    }
    ImGui::SameLine();
    // The strip between the two buttons works like a title bar.
    const float gap = ImGui::GetContentRegionAvail().x - ImGui::GetFrameHeight() - ImGui::GetStyle().ItemSpacing.x;
    if (gap > 1) {
        ImGui::InvisibleButton("viewport-strip", {gap, ImGui::GetFrameHeight()});
        toggleMaximize = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
        ImGui::SameLine();
    }
    if (EditorIconButton("maximize-viewport", maximized ? EditorGlyph::Restore : EditorGlyph::Expand,
                         maximized ? "Restore workspace" : "Maximize viewport"))
        toggleMaximize = true;
    return toggleMaximize;
}

EditorViewportInteraction ViewportPanel::Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                                              const ViewportImage& image, const std::vector<std::string>& recent,
                                              bool metrics, bool maximized, bool allowMarkGesture) {
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
    if (snapshot.ready && !snapshot.inspection.tables.empty()) {
        const float tableHeight = snapshot.inspection.showImage ? ImGui::GetContentRegionAvail().y * 0.65f : 0.0f;
        ImGui::BeginChild("native-content", {0, tableHeight}, ImGuiChildFlags_Borders);
        const auto& tables = snapshot.inspection.tables;
        _dataView = std::min(_dataView, tables.size() - 1);
        for (std::size_t index = 0; index < tables.size(); ++index) {
            if (index > 0) ContinueToolToolbar(tables[index].name.c_str());
            ImGui::PushID(static_cast<int>(index));
            if (ToolSegment(tables[index].name.c_str(), index == _dataView)) _dataView = index;
            ImGui::PopID();
        }
        const auto& table = tables[_dataView];
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
        _shownScale = scale;
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
        const bool gesture = allowMarkGesture && io.KeyAlt && !navigating;
        const bool cancelMark =
            ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
        const Float2 mouse{io.MousePos.x, io.MousePos.y};
        if (gesture && !cancelMark && result.hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
            _markStart = mouse;
            if (snapshot.ready && !snapshot.paused && snapshot.capabilities.Can(EditorCommand::Pause))
                result.pauseForMarking = true;
        }
        if (cancelMark || !allowMarkGesture) _markStart.reset();
        result.markDragging = _markStart.has_value();
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
        _shownScale = 0.0f;
        const char* title = snapshot.ready ? "Waiting for a frame" : EditorPhaseLabel(snapshot.phase);
        const char* detail = snapshot.status.c_str();
        if (!snapshot.selected) {
            title = "Choose a scene";
            detail = "Pick one from the Scenes panel.";
        } else if (snapshot.phase == EditorPhase::Error) {
            title = "Scene unavailable";
            detail = "See Output for details, then retry or build and refresh.";
        } else if (snapshot.phase == EditorPhase::Stopped) {
            title = "Scene stopped";
            detail = "Run the selected scene when you are ready.";
        }
        const char* button = nullptr;
        EditorCommand command = EditorCommand::Open;
        if (snapshot.selected && snapshot.capabilities.Can(EditorCommand::Open)) {
            button = snapshot.phase == EditorPhase::Error ? "Retry scene" : "Run scene";
        } else if (snapshot.capabilities.Can(EditorCommand::Cancel)) {
            button = "Cancel";
            command = EditorCommand::Cancel;
        }
        // The scenes opened last, when the viewport is waiting for a choice.
        std::vector<const SceneDefinition*> scenes;
        if (!snapshot.busy && (!snapshot.selected || snapshot.phase == EditorPhase::Stopped))
            for (const auto& id : recent) {
                if (scenes.size() == RecentRows) break;
                if (snapshot.selected && snapshot.selected->id == id) continue;
                const auto found = std::ranges::find(snapshot.catalog, id, &SceneDefinition::id);
                if (found != snapshot.catalog.end()) scenes.push_back(&*found);
            }
        const auto choice = DrawEmptyViewport(title, detail, button, command == EditorCommand::Open, scenes,
                                              !snapshot.selected, available);
        if (choice.button) actions.command(command);
        if (choice.recent) {
            actions.select(choice.recent->id);
            actions.command(EditorCommand::Open);
        }
    }
    DrawNotice(snapshot, cursor);
    return result;
}
