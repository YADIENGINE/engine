#include "Editor/Ui/Panels/Console/ConsoleViews.h"
#include <algorithm>

TerminalView& ConsoleViews::View(std::uint64_t id) {
    auto& view = _views[id];
    // Editor shortcuts stay available while the terminal owns keyboard input: the panels', the palette's and the
    // Agents drawer's (EditorWorkspace::Shortcuts).
    if (!view) {
        std::vector<ImGuiKeyChord> reserved{ImGuiMod_Ctrl | ImGuiKey_J,
                                            ImGuiMod_Ctrl | ImGuiKey_GraveAccent,
                                            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P,
                                            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A,
                                            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S,
                                            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N,
                                            ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_T,
                                            ImGuiMod_Ctrl | ImGuiKey_PageDown,
                                            ImGuiMod_Ctrl | ImGuiKey_PageUp};
        for (int index = 0; index < 9; ++index)
            reserved.push_back(ImGuiMod_Ctrl | static_cast<ImGuiKey>(ImGuiKey_1 + index));
        view = std::make_unique<TerminalView>(std::move(reserved));
    }
    return *view;
}
void ConsoleViews::Prune(const std::vector<ConsoleSessionInfo>& sessions) {
    std::erase_if(_views, [&](const auto& entry) {
        return std::ranges::none_of(sessions, [&](const ConsoleSessionInfo& info) { return info.id == entry.first; });
    });
}
