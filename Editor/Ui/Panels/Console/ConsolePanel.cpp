#include "Editor/Ui/Panels/Console/ConsolePanel.h"
#include <cstdint>
#include "ToolUi/ToolTheme.h"

namespace {
std::uint32_t ToRgb(const ImVec4& color) {
    const auto channel = [](float value) { return static_cast<std::uint32_t>(value * 255.0f + 0.5f) & 0xFF; };
    return (channel(color.x) << 16) | (channel(color.y) << 8) | channel(color.z);
}
TerminalPalette ConsolePalette() {
    TerminalPalette palette;
    palette.background = ToRgb(ToolPalette::Window);
    palette.foreground = ToRgb(ToolPalette::Text);
    return palette;
}
} // namespace

// Editörün kendi kısayolları konsol odaktayken de çalışır: alt alan, konsol, komut paleti.
ConsolePanel::ConsolePanel()
    : _session(ConsolePalette())
    , _view({ImGuiMod_Ctrl | ImGuiKey_J, ImGuiMod_Ctrl | ImGuiKey_GraveAccent,
             ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P}) {}

void ConsolePanel::Draw() {
    if (!_session.Started()) _session.Start({.directory = _directory});
    _view.Draw(_session);
}
