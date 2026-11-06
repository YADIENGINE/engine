#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <vector>
#include "Editor/Ui/Panels/Console/ConsoleSessions.h"
#include "Terminal/TerminalView.h"

// One TerminalView per Console session (scroll, selection and size of its screen), shared by the places that draw a
// session's terminal: the Console panel and the Agents drawer. A terminal is drawn in one place per frame; the other
// shows where it is (ConsolePanel::Draw's `elsewhere`), so two sizes never fight over one screen.
class ConsoleViews {
public:
    [[nodiscard]] TerminalView& View(std::uint64_t id);
    // Both hosts keep terminal selection at its Cabinet color, independent of the editor palette.
    void Draw(std::uint64_t id, TerminalSession& terminal);
    // Forgets the views of sessions that are gone.
    void Prune(const std::vector<ConsoleSessionInfo>& sessions);

private:
    std::map<std::uint64_t, std::unique_ptr<TerminalView>> _views;
};
