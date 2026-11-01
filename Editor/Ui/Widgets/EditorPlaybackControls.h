#pragma once
#include "Editor/Application/EditorPresentation.h"

// Draws an adaptive transport group and its overflow; the containing shell owns the toolbar row. `marking` is the Mark
// toggle: while on, a press on the scene marks a region for the Console instead of reaching the product.
void DrawEditorPlaybackControls(const EditorSnapshot& snapshot, const EditorUiActions& actions, bool& marking);
