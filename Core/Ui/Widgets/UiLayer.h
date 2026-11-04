#pragma once
#include "Ui/Kernel/Ui.h"

// A modal sheet with a dimmed backdrop that fades with it. Close it by passing 'open' as false:
//     if (auto pause = UiLayer(ui, "pause", _paused)) {
//         if (ui.Back()) _paused = false;
//         ...
//     }
[[nodiscard]] UiLayerScope UiLayer(Ui& ui, UiId id, bool open, UiLayerSpec spec = {});
