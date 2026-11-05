#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include <extras/IconsFontAwesome6.h>
#include <imgui_internal.h>
#include <algorithm>
#include <format>
#include "Editor/Ui/Style/EditorTheme.h"
#include "ToolUi/ToolTheme.h"
#include "ToolUi/ToolWidgets.h"

std::optional<std::uint64_t> DrawCloseAgentDialog(std::optional<std::uint64_t>& pending) {
    std::optional<std::uint64_t> closing;
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({380 * ToolUiScale(), 0}, ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(CloseAgentDialogId, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Closing this session ends its running program. Any work in progress will be interrupted.");
        ImGui::Separator();
        if (ToolButton("Close session", {}, true)) {
            closing = pending;
            pending.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ToolButton("Keep open") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
            pending.reset();
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
    return closing;
}
bool ConsolePanel::DrawSessionRow(ConsoleSessions& sessions, const ConsoleSessionInfo& info) {
    ImGui::PushID(std::to_string(info.id).c_str());
    const auto start = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float height = ImGui::GetFrameHeight();
    const float scale = ToolUiScale();
    const float gap = ToolMetrics::Space * scale;
    const float iconWidth = ImGui::GetFontSize();
    const ImVec2 end{start.x + width, start.y + height};
    const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
                         ImGui::IsMouseHoveringRect(start, end);
    auto* draw = ImGui::GetWindowDrawList();
    if (info.selected || hovered)
        draw->AddRectFilled(start, end, ImGui::GetColorU32(info.selected ? ToolPalette::Selected : ToolPalette::Hover),
                            ToolMetrics::Radius * ToolUiScale());
    // State and close share one trailing slot; closing a row does not also select it.
    ImGui::PushStyleColor(ImGuiCol_Header, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0, 0, 0, 0});
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {0, ImGui::GetStyle().ItemSpacing.y});
    if (ImGui::Selectable("##session", info.selected, 0, {std::max(1.0f, width - height), height}))
        sessions.Select(info.id);
    ImGui::PopStyleColor(3);
    const auto detail = info.title + "\n" + SessionStateName(info.state) +
                        (info.activity.empty() ? std::string() : "\n" + info.activity);
    ToolTooltip(detail.c_str());
    const bool exited = info.state == SessionState::Exited;
    const auto textColor = exited ? ToolPalette::Disabled : ToolPalette::Text;
    const auto kindColor = exited          ? ToolPalette::Disabled
                           : info.selected ? ToolPalette::Accent
                                           : ToolPalette::Secondary;
    const float textY = start.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
    draw->AddText({start.x + gap * 2, textY}, ImGui::GetColorU32(kindColor),
                  info.agent ? ICON_FA_ROBOT : ICON_FA_TERMINAL);
    const float trailingX = end.x - height;
    const float titleX = start.x + iconWidth + gap * 3;
    const float titleEnd = trailingX - gap;
    if (titleEnd > titleX) {
        ImGui::PushStyleColor(ImGuiCol_Text, textColor);
        ImGui::RenderTextEllipsis(draw, {titleX, textY}, {titleEnd, textY + ImGui::GetTextLineHeight()}, titleEnd,
                                  info.title.c_str(), nullptr, nullptr);
        ImGui::PopStyleColor();
    }
    ImGui::SameLine(0, 0);
    bool close = false;
    if (hovered || ImGui::IsItemFocused()) {
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0);
        ImGui::PushStyleColor(ImGuiCol_Text, ToolPalette::Secondary);
        close = ToolIconButton("close", ICON_FA_XMARK, "Close session");
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    } else {
        ImGui::Dummy({height, height});
        const auto* icon = EditorSessionStateIcon(info.state);
        draw->AddText({trailingX + (height - ImGui::CalcTextSize(icon).x) * 0.5f, textY},
                      ImGui::GetColorU32(EditorSessionStateColor(info.state)), icon);
    }
    ImGui::PopStyleVar();
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) ImGui::OpenPopup("session-actions");
    if (ImGui::BeginPopup("session-actions")) {
        close = ImGui::MenuItem("Close") || close;
        ImGui::EndPopup();
    }
    ImGui::PopID();
    return close;
}
ConsoleRequest ConsolePanel::Draw(ConsoleSessions& sessions, ConsoleViews& views,
                                  std::optional<std::uint64_t> elsewhere) {
    if (sessions.Empty()) sessions.OpenShell();
    _focused = false;
    std::optional<std::uint64_t> closing;
    ConsoleRequest request;
    bool showClose = false;
    const auto infos = sessions.Infos();
    views.Prune(infos);
    const auto origin = ImGui::GetCursorPos();
    const auto originScreen = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    const float scale = ToolUiScale();
    const float fontSize = ImGui::GetFontSize();
    const float splitterWidth = ToolMetrics::Space * scale;
    const float maxWidth = std::max(1.0f, available.x - splitterWidth - fontSize * 6);
    constexpr float NamedRowWidthInFonts = 11.0f; // Room for a session's icon, name and state
    const float minWidth = std::min(fontSize * NamedRowWidthInFonts, maxWidth);
    if (_sessionListWidth <= 0) _sessionListWidth = minWidth / scale;
    float listWidth = std::clamp(_sessionListWidth * scale, minWidth, maxWidth);
    float terminalWidth = std::max(1.0f, available.x - listWidth - splitterWidth);
    ImGui::SetCursorPos({origin.x + terminalWidth, origin.y});
    ImGui::InvisibleButton("console-session-splitter", {splitterWidth, std::max(1.0f, available.y)});
    const bool resizing = ImGui::IsItemActive();
    const bool resizeHovered = ImGui::IsItemHovered();
    if (resizing || resizeHovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (resizing) {
        listWidth = std::clamp(listWidth - ImGui::GetIO().MouseDelta.x, minWidth, maxWidth);
        _sessionListWidth = listWidth / scale;
        terminalWidth = std::max(1.0f, available.x - listWidth - splitterWidth);
    }
    const float splitterX = originScreen.x + terminalWidth + splitterWidth * 0.5f;
    ImGui::GetWindowDrawList()->AddLine(
        {splitterX, originScreen.y}, {splitterX, originScreen.y + available.y},
        ImGui::GetColorU32(resizing || resizeHovered ? ToolPalette::Accent : ToolPalette::Separator));
    ImGui::SetCursorPos({origin.x + terminalWidth + splitterWidth, origin.y});
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0);
    if (BeginToolPanel("console-session-list", {listWidth, available.y}, ToolSurface::Utility, 0,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse, ToolMetrics::Space)) {
        const float buttonWidth = ImGui::GetContentRegionAvail().x;
        ImGui::PushStyleColor(ImGuiCol_Text, ToolToneColor(ToolTone::Accent));
        const bool add = ToolButton(ICON_FA_PLUS "###console-add-terminal",
                                    {buttonWidth, EditorToolbarMetrics::ControlHeight * scale});
        ImGui::PopStyleColor();
        ToolTooltip("Add terminal");
        if (add) ImGui::OpenPopup("Add terminal");
        if (ImGui::BeginPopup("Add terminal")) {
            if (ImGui::MenuItem(ICON_FA_TERMINAL "  Shell")) request.add = ConsoleNewSession{};
            for (const auto& kind : AgentKinds) {
                const auto label = std::string(ICON_FA_ROBOT "  ") +
                                   (kind.id == AgentKind::Custom ? "Custom agent" : kind.sessionName);
                if (ImGui::MenuItem(label.c_str(), kind.command)) request.add = ConsoleNewSession{kind.id};
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        if (ImGui::BeginChild("sessions")) {
            for (const auto& info : infos) {
                if (DrawSessionRow(sessions, info)) {
                    if (info.agent && info.state != SessionState::Exited) {
                        _confirmClose = info.id;
                        showClose = true;
                    } else {
                        closing = info.id;
                    }
                }
            }
        }
        ImGui::EndChild();
    }
    EndToolPanel();
    ImGui::PopStyleVar(2);
    if (showClose) ImGui::OpenPopup(CloseAgentDialogId);
    if (const auto confirmed = DrawCloseAgentDialog(_confirmClose)) closing = confirmed;
    if (closing) sessions.Close(*closing);
    ImGui::SetCursorPos(origin);
    if (ImGui::BeginChild("console-terminal", {terminalWidth, available.y}, ImGuiChildFlags_AlwaysUseWindowPadding,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse) &&
        sessions.Selected()) {
        const auto id = *sessions.Selected();
        if (elsewhere && *elsewhere == id) {
            // The Agents drawer shows this terminal now; one screen has one size.
            const auto info = sessions.Info(id);
            const auto title = (info ? info->title : std::string("This session")) + " is open in Agents";
            ToolEmptyState(title.c_str(), "Its terminal is in the Agents drawer.");
            const char* label = "Show here";
            const float width = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - width) * 0.5f);
            if (ToolButton(label)) request.showHere = true;
        } else if (auto* terminal = sessions.Terminal(id)) {
            auto& view = views.View(id);
            view.Draw(*terminal);
            _focused = view.Focused();
        }
    }
    ImGui::EndChild();
    return request;
}
