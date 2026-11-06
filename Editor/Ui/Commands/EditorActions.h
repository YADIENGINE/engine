#pragma once
#include <imgui.h>
#include <array>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include "Editor/Ui/Shell/EditorShell.h"

// The main menu bar's menus, in their order.
enum class EditorMenu { File, Scene, View, Build, Git, Agent, Help, Count };
enum class EditorActionCheck { None, Toggle, Radio };

// clang-format off
// Every editor shortcut. One key runs at most one action of EditorActionList.
enum class EditorKey { None, QuickOpen, CommandPalette, Console, BottomTool, Agents, SendScene, NewAgent, GitChanges, Primary, Pause, Step, Reset, Build, NextAgent, PreviousAgent, AgentView, Agent1, Agent2, Agent3, Agent4, Agent5, Agent6, Agent7, Agent8, Agent9 };
// clang-format on
// When a key works; never while a popup is open.
// Everywhere: also while typing or while the Console's terminal has the keyboard (it leaves these keys to the editor).
// OutsideConsole: not while the Console has the keyboard. Playback: neither while typing nor in the Console.
// AgentsDrawer: only while the Agents drawer is open; closed, the key stays free for whatever has the keyboard.
enum class EditorKeyScope { Everywhere, OutsideConsole, Playback, AgentsDrawer };

struct EditorKeyBinding {
    EditorKey key;
    ImGuiKeyChord chord;
    const char* text; // As menus and the shortcut list show it
    EditorKeyScope scope;
    const char* name = nullptr; // In the shortcut list, for keys no menu shows; nullptr: listed under its action
};

// The key table: the menus' shortcut text, the keys the workspace listens to and those the Console leaves alone all
// come from it.
[[nodiscard]] std::span<const EditorKeyBinding> EditorKeyBindings();
[[nodiscard]] const EditorKeyBinding* EditorKeyInfo(EditorKey key);
// Keys the Console's terminal hands to the editor: those that work everywhere and the Agents drawer's.
[[nodiscard]] std::vector<ImGuiKeyChord> EditorTerminalReservedKeys();

// One thing the editor can do. The menu bar, the command palette, the keyboard shortcut list and the workspace's key
// handling all read the same list (EditorActionList), so an action, its shortcut and why it is unavailable read the
// same everywhere (Docs/Editor/Usage.md → Menus).
struct EditorAction {
    EditorMenu menu = EditorMenu::File;
    const char* section = "";     // A menu draws a separator where its section changes
    const char* submenu = "";     // Non-empty: drawn inside this submenu, which keeps one section
    bool separatorBefore = false; // Inside a submenu
    std::string label;
    EditorKey key = EditorKey::None;
    const char* shortcut = ""; // Filled from the key table
    bool enabled = true;
    std::string reason;  // Why it is unavailable
    std::string tooltip; // What it does, when the label needs more
    EditorActionCheck check = EditorActionCheck::None;
    bool checked = false;
    bool palette = true;           // Also offered by the command palette
    bool inMenu = true;            // False: only its key runs it (the Agents drawer's navigation)
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
    bool offerProjectList = false; // Only when a launcher started this editor (Docs/ToolsContract.md)
    bool agentsDrawerOpen = false;
    std::filesystem::path engineDirectory; // Help → Editor guide opens its Docs/ copy when present
    std::string agentContextTarget = "New agent session";
    std::string sceneContextTarget = "New agent session";
    std::string sceneContextUnavailable;
    EditorShellResult& result;
};

[[nodiscard]] const char* EditorMenuTitle(EditorMenu menu);
[[nodiscard]] std::vector<EditorAction> EditorActionList(const EditorActionContext& context);
// The editor guide: the engine's own copy when it ships Docs/, else the page for this engine version online.
[[nodiscard]] std::filesystem::path EditorGuideLocation(const std::filesystem::path& engineDirectory);
