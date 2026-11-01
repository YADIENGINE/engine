#pragma once
#include "Terminal/TerminalTypes.h"

// ImGui keyboard modifiers normalized for terminal input, including GLFW's RightAlt-only AltGr.
[[nodiscard]] TerminalModifiers ReadTerminalModifiers();
