#include "Terminal/Internals/TerminalInput.h"
#include <imgui.h>

TerminalModifiers ReadTerminalModifiers() {
    const auto& io = ImGui::GetIO();
    // GLFW suppresses AltGr's synthetic LeftCtrl event: RightAlt alone is also text input.
    const bool altGr = ImGui::IsKeyDown(ImGuiKey_RightAlt) || (io.KeyCtrl && io.KeyAlt);
    return {.shift = io.KeyShift, .alt = io.KeyAlt && !altGr, .ctrl = io.KeyCtrl && !altGr};
}
