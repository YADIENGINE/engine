#pragma once
#include "Ui/Kernel/Ui.h"
#include "Ui/Layout/UiLayout.h"

struct UiScrollOptions {
    UiSize width = UiFill();
    UiSize height = UiFill();
};

// An area that scrolls up and down when its content is taller than it. It follows a keyboard cursor, and takes the
// wheel and a drag on its bar. Elements added while it lives are its content, arranged by 'content':
//     auto list = UiScroll(ui, "items", {.gap = 4.0f});
//     for (const Item& item : items) ...
//     auto grid = UiScroll(ui, "tiles", {.direction = UiDirection::Grid, .columns = 4, .gap = 8.0f});
// Every item is described every frame, so for very long lists describe only those near the view.
class [[nodiscard]] UiScroll {
public:
    UiScroll(Ui& ui, UiId id, UiBox content = {}, const UiScrollOptions& options = {});
    // Ends it here, as the end of its scope would.
    void Close();
    [[nodiscard]] float Offset() const { return _offset; }

private:
    UiElement _viewport;
    float _offset = 0.0f;
    UiElement _content;
};
