#pragma once
#include <span>
#include "Ui/Layout/UiLayout.h"
#include "Utilities/FloatRect.h"

// One element. Parents come before their children, in the order they were added.
struct UiLayoutNode {
    UiBox box{};
    int parent = -1;
    int firstChild = -1;
    int lastChild = -1;
    int nextSibling = -1;
    const UiMeasure* measure = nullptr; // Borrowed for the solve
    FloatRect rect{};                   // Result
};

// Fills every node's rect. A node without a parent takes the whole 'canvas'. Widths are settled before heights, because
// a wrapped text's height depends on its width.
void SolveUiLayout(std::span<UiLayoutNode> nodes, const FloatRect& canvas);
