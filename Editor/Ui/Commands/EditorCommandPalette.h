#pragma once
#include <array>
#include "Editor/Ui/Shell/EditorShell.h"

class EditorCommandPalette {
public:
    // True while the palette owns input, including the frame that executes a command.
    [[nodiscard]] bool Draw(const EditorSnapshot& snapshot, const EditorUiActions& actions,
                            EditorPreferences& preferences, bool maximized, EditorShellResult& result);

private:
    std::array<char, 256> _search{};
    int _selected = 0;
    bool _focusSearch = false;
    bool _wasOpen = false;
};
