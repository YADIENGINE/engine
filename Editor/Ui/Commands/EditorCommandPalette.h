#pragma once
#include <array>
#include <filesystem>
#include <string>
#include <vector>
#include "Editor/Ui/Commands/EditorActions.h"

// One search over the project: Ctrl+P finds scenes, their choices, graphics, sounds and commands; Ctrl+Shift+P the
// same window with commands only.
class EditorCommandPalette {
public:
    void SetAssetRoots(const std::filesystem::path& engineDirectory, const std::filesystem::path& assetsDirectory);
    // Where it appears: the toolbar's search box.
    void SetAnchor(float centerX, float top) {
        _anchorCenterX = centerX;
        _anchorTop = top;
    }
    // True while the palette owns input, including the frame that executes a command.
    // Commands come from EditorActionList, the same list the menus draw.
    [[nodiscard]] bool Draw(const EditorActionContext& context);

private:
    struct Sound {
        std::string name;
        std::string folder;
        std::filesystem::path path;
    };
    void ReadSounds();
    std::filesystem::path _engineDirectory;
    std::filesystem::path _assetsDirectory;
    std::vector<Sound> _sounds; // Read from disk each time the palette opens
    float _anchorCenterX = 0;
    float _anchorTop = 0;
    std::array<char, 256> _search{};
    int _selected = 0;
    bool _commandsOnly = false;
    bool _focusSearch = false;
    bool _wasOpen = false;
};
