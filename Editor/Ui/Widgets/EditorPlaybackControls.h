#pragma once
#include "Editor/Application/EditorPresentation.h"

// What the toolbar's search box did, and where it stands so quick open can appear over it.
struct EditorToolbarSearch {
    bool pressed = false;
    float centerX = 0;
    float top = 0;
};

// Draws the toolbar row: playback at the left, the search box in the middle, tools and their overflow at the right. The
// containing shell owns the row. `marking` is the Mark toggle: while on, a press marks a region for the Console instead
// of reaching the product; it needs no scene, since anything in the window can be marked.
void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool& marking,
                                EditorToolbarSearch& search);
