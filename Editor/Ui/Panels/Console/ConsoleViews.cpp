#include "Editor/Ui/Panels/Console/ConsoleViews.h"
#include <algorithm>
#include "Editor/Ui/Commands/EditorActions.h"

TerminalView& ConsoleViews::View(std::uint64_t id) {
    auto& view = _views[id];
    // Editor shortcuts stay available while the terminal owns keyboard input: the panels', the palette's and the
    // Agents drawer's (EditorKeyBindings).
    if (!view) view = std::make_unique<TerminalView>(EditorTerminalReservedKeys());
    return *view;
}
void ConsoleViews::Prune(const std::vector<ConsoleSessionInfo>& sessions) {
    std::erase_if(_views, [&](const auto& entry) {
        return std::ranges::none_of(sessions, [&](const ConsoleSessionInfo& info) { return info.id == entry.first; });
    });
}
