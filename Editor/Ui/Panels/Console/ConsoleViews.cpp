#include "Editor/Ui/Panels/Console/ConsoleViews.h"
#include <algorithm>
#include "Editor/Ui/Commands/EditorActions.h"
#include "ToolUi/ToolTheme.h"

TerminalView& ConsoleViews::View(std::uint64_t id) {
    auto& view = _views[id];
    // Editor shortcuts stay available while the terminal owns keyboard input: the panels', the palette's and the
    // Agents drawer's (EditorKeyBindings).
    if (!view) view = std::make_unique<TerminalView>(EditorTerminalReservedKeys());
    return *view;
}
void ConsoleViews::Draw(std::uint64_t id, TerminalSession& terminal) {
    const auto accent = ToolPalette::Accent;
    ImGui::PushStyleColor(ImGuiCol_TextSelectedBg, ImVec4{accent.x, accent.y, accent.z, 0.25f});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, ImVec4{0, 0, 0, 0});
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, ToolPalette::Control);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ToolPalette::Hover);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, ToolPalette::Muted);
    View(id).Draw(terminal);
    ImGui::PopStyleColor(5);
}
void ConsoleViews::Prune(const std::vector<ConsoleSessionInfo>& sessions) {
    std::erase_if(_views, [&](const auto& entry) {
        return std::ranges::none_of(sessions, [&](const ConsoleSessionInfo& info) { return info.id == entry.first; });
    });
}
