#pragma once
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include "Editor/Ui/Shell/EditorShell.h"

// The main menu bar's menus, in their order.
enum class EditorMenu { File, Scene, View, Build, Git, Agent, Help, Count };
enum class EditorActionCheck { None, Toggle, Radio };

// One thing the editor can do. The menu bar, the command palette and the keyboard shortcut list all read the same list
// (EditorActionList), so an action, its shortcut and why it is unavailable read the same everywhere
// (Docs/Editor/Usage.md → Menus).
struct EditorAction {
    EditorMenu menu = EditorMenu::File;
    const char* section = "";     // A menu draws a separator where its section changes
    const char* submenu = "";     // Non-empty: drawn inside this submenu, which keeps one section
    bool separatorBefore = false; // Inside a submenu
    std::string label;
    const char* shortcut = "";
    bool enabled = true;
    std::string reason;  // Why it is unavailable
    std::string tooltip; // What it does, when the label needs more
    EditorActionCheck check = EditorActionCheck::None;
    bool checked = false;
    bool palette = true;           // Also offered by the command palette
    std::function<void()> run;     // An item
    std::function<void()> content; // Instead: a submenu drawn by its owner (menu only)
};

// What the actions read and change for one frame. Every action writes its requests into `result`.
struct EditorActionContext {
    const EditorSnapshot& snapshot;
    const EditorUiActions& actions;
    EditorPreferences& preferences;
    int& fontSize;
    bool maximized = false;
    bool offerProjectList = false;         // Only when a launcher started this editor (Docs/ToolsContract.md)
    std::filesystem::path engineDirectory; // Help → Editor guide opens its Docs/ copy when present
    EditorShellResult& result;
};

[[nodiscard]] const char* EditorMenuTitle(EditorMenu menu);
[[nodiscard]] std::vector<EditorAction> EditorActionList(const EditorActionContext& context);
// The editor guide: the engine's own copy when it ships Docs/, else the page for this engine version online.
[[nodiscard]] std::filesystem::path EditorGuideLocation(const std::filesystem::path& engineDirectory);
