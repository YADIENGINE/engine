#include "Editor/Ui/Commands/EditorActions.h"
#include <algorithm>
#include <string>
#include <system_error>
#include "Editor/Internals/Agent/AgentProfile.h"
#include "Editor/Ui/Commands/EditorCommands.h"
#include "Editor/Ui/Style/EditorPalettePreset.h"
#include "Editor/Ui/Widgets/EditorAgentSetup.h"

namespace {
// clang-format off
constexpr std::array KeyBindings{
    EditorKeyBinding{EditorKey::QuickOpen, ImGuiMod_Ctrl | ImGuiKey_P, "Ctrl+P", EditorKeyScope::OutsideConsole},
    EditorKeyBinding{EditorKey::CommandPalette, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P, "Ctrl+Shift+P", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::Console, ImGuiMod_Ctrl | ImGuiKey_GraveAccent, "Ctrl+`", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::BottomTool, ImGuiMod_Ctrl | ImGuiKey_J, "Ctrl+J", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::Agents, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_A, "Ctrl+Shift+A", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::SendScene, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_S, "Ctrl+Shift+S", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::NewAgent, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_N, "Ctrl+Shift+N", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::GitChanges, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_G, "Ctrl+Shift+G", EditorKeyScope::Everywhere},
    EditorKeyBinding{EditorKey::Primary, ImGuiKey_F5, "F5", EditorKeyScope::Playback},
    EditorKeyBinding{EditorKey::Pause, ImGuiKey_F6, "F6", EditorKeyScope::Playback},
    EditorKeyBinding{EditorKey::Step, ImGuiKey_F7, "F7", EditorKeyScope::Playback},
    EditorKeyBinding{EditorKey::Reset, ImGuiMod_Ctrl | ImGuiKey_R, "Ctrl+R", EditorKeyScope::Playback},
    EditorKeyBinding{EditorKey::Build, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_B, "Ctrl+Shift+B", EditorKeyScope::Playback},
    EditorKeyBinding{EditorKey::NextAgent, ImGuiMod_Ctrl | ImGuiKey_PageDown, "Ctrl+PageDown", EditorKeyScope::AgentsDrawer, "Next agent"},
    EditorKeyBinding{EditorKey::PreviousAgent, ImGuiMod_Ctrl | ImGuiKey_PageUp, "Ctrl+PageUp", EditorKeyScope::AgentsDrawer, "Previous agent"},
    EditorKeyBinding{EditorKey::AgentView, ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_T, "Ctrl+Shift+T", EditorKeyScope::AgentsDrawer, "Terminal / Activity"},
    EditorKeyBinding{EditorKey::Agent1, ImGuiMod_Ctrl | ImGuiKey_1, "Ctrl+1 ... Ctrl+9", EditorKeyScope::AgentsDrawer, "Select agent 1-9"},
    EditorKeyBinding{EditorKey::Agent2, ImGuiMod_Ctrl | ImGuiKey_2, "Ctrl+2", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent3, ImGuiMod_Ctrl | ImGuiKey_3, "Ctrl+3", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent4, ImGuiMod_Ctrl | ImGuiKey_4, "Ctrl+4", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent5, ImGuiMod_Ctrl | ImGuiKey_5, "Ctrl+5", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent6, ImGuiMod_Ctrl | ImGuiKey_6, "Ctrl+6", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent7, ImGuiMod_Ctrl | ImGuiKey_7, "Ctrl+7", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent8, ImGuiMod_Ctrl | ImGuiKey_8, "Ctrl+8", EditorKeyScope::AgentsDrawer, ""},
    EditorKeyBinding{EditorKey::Agent9, ImGuiMod_Ctrl | ImGuiKey_9, "Ctrl+9", EditorKeyScope::AgentsDrawer, ""}};
// clang-format on
EditorKey CommandKey(EditorCommand command, const EditorSnapshot& snapshot) {
    if (command == EditorPrimaryCommand(snapshot)) return EditorKey::Primary;
    switch (command) {
    case EditorCommand::Pause  : return EditorKey::Pause;
    case EditorCommand::Step   : return EditorKey::Step;
    case EditorCommand::Reset  : return EditorKey::Reset;
    case EditorCommand::Refresh: return EditorKey::Build;
    default                    : return EditorKey::None;
    }
}
// Menus name what a command covers; the toolbar keeps its short labels.
std::string MenuLabel(EditorCommand command, const EditorSnapshot& snapshot) {
    switch (command) {
    case EditorCommand::Open        : return "Run scene";
    case EditorCommand::Stop        : return "Stop scene";
    case EditorCommand::Refresh     : return snapshot.building ? "Building..." : "Build & Refresh";
    case EditorCommand::BuildAndPlay: return "Build & Play game";
    default                         : return EditorCommandLabel(command, snapshot);
    }
}
const char* CommandTooltip(EditorCommand command) {
    switch (command) {
    case EditorCommand::Open : return "Start the selected scene in its worker.";
    case EditorCommand::Reset: return "A new instance of the scene from its accepted values; the clock starts at 0.";
    case EditorCommand::Refresh:
        return "Build the scene worker; the open scene keeps running and is replaced when ready.";
    case EditorCommand::BuildAndPlay: return "Build the game and start it in its own window.";
    default                         : return "";
    }
}
EditorAction Command(EditorMenu menu, const char* section, EditorCommand command, const EditorActionContext& context) {
    const auto& snapshot = context.snapshot;
    EditorAction action;
    action.menu = menu;
    action.section = section;
    action.label = MenuLabel(command, snapshot);
    action.key = CommandKey(command, snapshot);
    action.enabled = snapshot.capabilities.Can(command);
    action.reason = action.enabled ? "" : snapshot.capabilities.Reason(command);
    action.tooltip = CommandTooltip(command);
    action.checked = command == EditorCommand::Pause && snapshot.paused; // Its label already says Pause or Resume
    action.run = [&actions = context.actions, command] {
        if (actions.command) actions.command(command);
    };
    return action;
}
bool BuildFailed(const EditorSnapshot& snapshot) {
    const auto result = snapshot.diagnostics.Attempt().result;
    return result == EditorBuildResult::BuildFailed || result == EditorBuildResult::RefreshFailed;
}

void FileActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    auto& result = context.result;
    list.push_back({.menu = EditorMenu::File,
                    .section = "open",
                    .label = "Quick open",
                    .key = EditorKey::QuickOpen,
                    .tooltip = "Find a scene, one of its choices, a graphic, a sound or a command.",
                    .palette = false,
                    .run = [&result] { result.openQuick = true; }});
    if (context.offerProjectList)
        list.push_back({.menu = EditorMenu::File,
                        .section = "leave",
                        .label = "Back to project list",
                        .palette = false,
                        .run = [&result] { result.returnToProjectList = true; }});
    list.push_back({.menu = EditorMenu::File, .section = "leave", .label = "Exit", .palette = false, .run = [&result] {
                        result.quit = true;
                    }});
}

void SceneActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    const auto& snapshot = context.snapshot;
    const auto primary = EditorPrimaryCommand(snapshot);
    list.push_back(Command(EditorMenu::Scene, "run", primary, context));
    // A benchmark's primary is Cancel; the scene under it still stops on its own.
    if (snapshot.profiler.busy && primary != EditorCommand::Stop)
        list.push_back(Command(EditorMenu::Scene, "run", EditorCommand::Stop, context));
    list.push_back(Command(EditorMenu::Scene, "time", EditorCommand::Pause, context));
    list.push_back(Command(EditorMenu::Scene, "time", EditorCommand::Step, context));
    list.push_back(Command(EditorMenu::Scene, "time", EditorCommand::Reset, context));
}

void ViewActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    auto& result = context.result;
    auto& preferences = context.preferences;
    list.push_back({.menu = EditorMenu::View,
                    .section = "find",
                    .label = "Command palette",
                    .key = EditorKey::CommandPalette,
                    .palette = false,
                    .run = [&result] { result.openCommands = true; }});
    list.push_back({.menu = EditorMenu::View,
                    .section = "find",
                    .label = context.maximized ? "Restore workspace" : "Focus viewport",
                    .tooltip = "The scene fills the workspace; the panels come back with Restore workspace.",
                    .run = [&result] { result.toggleMaximize = true; }});
    for (const auto& panel : EditorPanels) {
        const auto id = panel.id;
        list.push_back({.menu = EditorMenu::View,
                        .section = "panels",
                        .submenu = "Panels",
                        .label = panel.title,
                        .key = id == EditorPanel::Console ? EditorKey::Console : EditorKey::None,
                        .check = EditorActionCheck::Toggle,
                        .checked = preferences.Visible(id),
                        .run = panel.dock == EditorDockGroup::Bottom
                                   ? std::function<void()>([&result, id] { result.bottomPanel = id; })
                                   : [&preferences, id] { preferences.Visible(id) = !preferences.Visible(id); }});
    }
    list.push_back({.menu = EditorMenu::View,
                    .section = "panels",
                    .submenu = "Panels",
                    .separatorBefore = true,
                    .label = "Toggle last bottom tool",
                    .key = EditorKey::BottomTool,
                    .run = [&result] { result.toggleBottom = true; }});
    for (const auto& layout : EditorLayouts) {
        const auto id = layout.id;
        const bool enabled = id != EditorWorkspaceLayout::Custom || !preferences.customDocking.empty();
        list.push_back({.menu = EditorMenu::View,
                        .section = "panels",
                        .submenu = "Layouts",
                        .separatorBefore = id == EditorWorkspaceLayout::Custom,
                        .label = layout.title,
                        .enabled = enabled,
                        .reason = enabled ? "" : "No custom layout has been saved.",
                        .check = EditorActionCheck::Radio,
                        .checked = preferences.layout == id,
                        .run = [&result, id] { result.layout = id; }});
    }
    for (const auto& palette : EditorPalettePresets())
        list.push_back({.menu = EditorMenu::View,
                        .section = "look",
                        .submenu = "Theme",
                        .label = palette.title,
                        .check = EditorActionCheck::Radio,
                        .checked = preferences.theme == palette.id,
                        .closeMenuOnSelect = false,
                        .run = [&preferences, id = palette.id] { preferences.theme = id; }});
    for (const int percent : EditorFontSizes)
        list.push_back({.menu = EditorMenu::View,
                        .section = "look",
                        .submenu = "Interface scale",
                        .label = std::to_string(percent) + "%",
                        .check = EditorActionCheck::Radio,
                        .checked = context.fontSize == percent,
                        .run = [&fontSize = context.fontSize, percent] { fontSize = percent; }});
    list.push_back({.menu = EditorMenu::View,
                    .section = "metrics",
                    .label = "Viewport performance metrics",
                    .check = EditorActionCheck::Toggle,
                    .checked = preferences.showMetrics,
                    .run = [&preferences] { preferences.showMetrics = !preferences.showMetrics; }});
}

void BuildActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    const auto& snapshot = context.snapshot;
    list.push_back(Command(EditorMenu::Build, "build", EditorCommand::Refresh, context));
    list.push_back(Command(EditorMenu::Build, "build", EditorCommand::BuildAndPlay, context));
    auto cancel = Command(EditorMenu::Build, "cancel", EditorCommand::Cancel, context);
    cancel.label = "Cancel build";
    cancel.key = EditorKey::None; // F5 belongs to Scene's primary command
    if (!snapshot.building) {
        cancel.enabled = false;
        cancel.reason = "No build is running.";
    }
    // Without a scene, Scene's primary command is this same Cancel: the palette lists it once.
    cancel.palette = EditorPrimaryCommand(snapshot) != EditorCommand::Cancel;
    list.push_back(std::move(cancel));
}

void GitActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    const auto& snapshot = context.snapshot;
    if (!snapshot.git) return;
    const auto& git = *snapshot.git;
    auto& result = context.result;
    std::string unavailable;
    if (git.availability == GitAvailability::NoRepository)
        unavailable = "This project is not a Git repository yet: Changes offers to create one.";
    if (git.availability == GitAvailability::Checking) unavailable = "Looking for Git...";
    if (git.availability == GitAvailability::Missing) unavailable = "Git is not installed or not on PATH.";
    const auto* repository = git.Active();
    const std::string busy = !git.busy.empty() ? "Git is busy: " + git.busy + "." : "";
    const auto add = [&](const char* section, const char* label, EditorKey key, std::string reason,
                         std::function<void()> run) {
        if (reason.empty() && key != EditorKey::GitChanges) reason = unavailable; // The drawer explains Git's state
        const bool enabled = reason.empty();
        list.push_back({.menu = EditorMenu::Git,
                        .section = section,
                        .label = label,
                        .key = key,
                        .enabled = enabled,
                        .reason = enabled ? "" : reason,
                        .run = std::move(run)});
    };
    const auto request = [&actions = context.actions](GitRequestKind kind) {
        return [&actions, kind] {
            if (actions.git) actions.git(GitRequest{.kind = kind});
        };
    };
    add("drawer", "Changes", EditorKey::GitChanges, "", [&result] { result.git.toggle = GitDrawerTab::Changes; });
    add("drawer", "Incoming commits", EditorKey::None, "", [&result] { result.git.show = GitDrawerTab::Incoming; });
    add("sync", "Fetch", EditorKey::None, busy, request(GitRequestKind::Fetch));
    std::string pull = busy;
    if (pull.empty()) {
        if (snapshot.building)
            pull = "Wait for the build to finish.";
        else if (!repository || repository->branch.behind == 0)
            pull = "Nothing to pull.";
        else if (repository->branch.ahead > 0)
            pull = "This branch and its remote both have new commits: resolve it in Changes.";
        else if (!repository->incoming.overlapping.empty())
            pull = "Incoming commits change files changed here: resolve it in Changes.";
    }
    add("sync", "Pull", EditorKey::None, pull, request(GitRequestKind::Pull));
    std::string push = busy;
    if (push.empty()) {
        if (!repository || repository->branch.ahead == 0)
            push = "Nothing to push.";
        else if (repository->branch.behind > 0)
            push = "The remote has new commits: pull first.";
    }
    add("sync", "Push", EditorKey::None, push, request(GitRequestKind::Push));
    std::string submodules = busy;
    if (submodules.empty() && git.repositories.size() < 2) submodules = "This project has no submodules.";
    add("sync", "Update submodules", EditorKey::None, submodules, request(GitRequestKind::UpdateSubmodules));
}

void AgentActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    const auto& snapshot = context.snapshot;
    auto& result = context.result;
    auto& agent = context.preferences.agent;
    const auto request = [&result](EditorAgentRequest kind) { return [&result, kind] { result.agent = kind; }; };
    const bool failed = BuildFailed(snapshot);
    const char* notFailed = "The last Build & Refresh did not fail.";
    list.push_back(
        {.menu = EditorMenu::Agent,
         .section = "context",
         .label = "Paste scene context to " + context.sceneContextTarget,
         .key = EditorKey::SendScene,
         .enabled = snapshot.ready && context.sceneContextUnavailable.empty(),
         .reason = snapshot.ready ? context.sceneContextUnavailable : "Run a scene before pasting its context.",
         .tooltip = "Paste the open scene, its time, changed values and a screenshot into the agent session: the "
                    "agent chosen in the Agents drawer while it is open. Paste only; you submit.",
         .run = context.agentsDrawerOpen ? std::function<void()>([&result] { result.agents.sendScene = true; })
                                         : request(EditorAgentRequest::SendScene)});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "context",
                    .label = "Paste build failure to " + context.agentContextTarget,
                    .enabled = failed,
                    .reason = failed ? "" : notFailed,
                    .tooltip = "Paste the last build failure into the agent session, without Enter.",
                    .run = request(EditorAgentRequest::SendBuildFailure)});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "context",
                    .label = "Ask agent to fix build with " + context.agentContextTarget,
                    .enabled = failed,
                    .reason = failed ? "" : notFailed,
                    .tooltip = "Send the build errors and ask the agent to fix them. This presses Enter.",
                    .run = request(EditorAgentRequest::FixBuild)});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "sessions",
                    .label = context.agentsDrawerOpen ? "Hide agents" : "Show agents",
                    .key = EditorKey::Agents,
                    .tooltip = "Every agent, its terminal and what it did in the editor.",
                    .run = [&result] { result.agents.toggle = true; }});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "sessions",
                    .label = "New agent",
                    .key = EditorKey::NewAgent,
                    .tooltip = "Start another agent session in the Agents drawer.",
                    .run = [&result] { result.agents.add = true; }});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "sessions",
                    .label = std::string("Open ") + AgentKindInfo(agent.kind).name,
                    .tooltip = "The agent session, started when there is none.",
                    .run = request(EditorAgentRequest::Open)});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "sessions",
                    .label = "New shell",
                    .tooltip = "A terminal in the project folder, in the Console.",
                    .run = request(EditorAgentRequest::NewShell)});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "settings",
                    .label = "Agent control",
                    .tooltip = "Allow agent tools (python Engine ctl) to change the editor.\n"
                               "Off: tools can still observe status, logs, parameters and screenshots.",
                    .check = EditorActionCheck::Toggle,
                    .checked = agent.control,
                    .run = [&agent] { agent.control = !agent.control; }});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "settings",
                    .label = "Visual checks",
                    .tooltip = "Allow agent tools to capture the viewport (screenshots, frame grids) and look at it.\n"
                               "Off: captures are refused and the agent tells you what to look at instead.",
                    .check = EditorActionCheck::Toggle,
                    .checked = agent.visualChecks,
                    .run = [&agent] { agent.visualChecks = !agent.visualChecks; }});
    list.push_back({.menu = EditorMenu::Agent,
                    .section = "settings",
                    .label = "Agent CLI",
                    .tooltip = "The agent program new sessions start.",
                    .palette = false,
                    .content = [&agent] { EditorAgentChoice(agent, ImGui::GetFontSize() * 26); }});
    // The drawer's navigation: keys only, while it is open.
    const auto drawerKey = [&](const char* label, EditorKey key, std::function<void()> run) {
        list.push_back({.menu = EditorMenu::Agent,
                        .section = "drawer",
                        .label = label,
                        .key = key,
                        .enabled = context.agentsDrawerOpen,
                        .reason = context.agentsDrawerOpen ? "" : "The Agents drawer is closed.",
                        .palette = false,
                        .inMenu = false,
                        .run = std::move(run)});
    };
    drawerKey("Next agent", EditorKey::NextAgent, [&result] { result.agents.step = 1; });
    drawerKey("Previous agent", EditorKey::PreviousAgent, [&result] { result.agents.step = -1; });
    drawerKey("Terminal / Activity", EditorKey::AgentView, [&result] { result.agents.switchView = true; });
    for (std::size_t index = 0; index < 9; ++index)
        drawerKey("Select agent", static_cast<EditorKey>(static_cast<int>(EditorKey::Agent1) + static_cast<int>(index)),
                  [&result, index] { result.agents.chip = index; });
}

void HelpActions(const EditorActionContext& context, std::vector<EditorAction>& list) {
    auto& result = context.result;
    list.push_back({.menu = EditorMenu::Help, .section = "help", .label = "Keyboard shortcuts", .run = [&result] {
                        result.showShortcuts = true;
                    }});
    const auto guide = EditorGuideLocation(context.engineDirectory);
    list.push_back({.menu = EditorMenu::Help,
                    .section = "help",
                    .label = "Editor guide",
                    .enabled = static_cast<bool>(context.actions.openPath),
                    .reason = context.actions.openPath ? "" : "This editor cannot open documents.",
                    .tooltip = "Opens " + guide.generic_string(),
                    .run = [&actions = context.actions, guide] { actions.openPath(guide); }});
    list.push_back({.menu = EditorMenu::Help, .section = "about", .label = "About " ENGINE_NAME, .run = [&result] {
                        result.showAbout = true;
                    }});
}
} // namespace

const char* EditorMenuTitle(EditorMenu menu) {
    switch (menu) {
    case EditorMenu::File : return "File";
    case EditorMenu::Scene: return "Scene";
    case EditorMenu::View : return "View";
    case EditorMenu::Build: return "Build";
    case EditorMenu::Git  : return "Git";
    case EditorMenu::Agent: return "Agent";
    case EditorMenu::Help : return "Help";
    case EditorMenu::Count: break;
    }
    return "";
}

std::vector<EditorAction> EditorActionList(const EditorActionContext& context) {
    std::vector<EditorAction> list;
    FileActions(context, list);
    SceneActions(context, list);
    ViewActions(context, list);
    BuildActions(context, list);
    GitActions(context, list);
    AgentActions(context, list);
    HelpActions(context, list);
    for (auto& action : list)
        if (const auto* binding = EditorKeyInfo(action.key)) action.shortcut = binding->text;
    return list;
}

std::span<const EditorKeyBinding> EditorKeyBindings() { return KeyBindings; }

const EditorKeyBinding* EditorKeyInfo(EditorKey key) {
    const auto found = std::ranges::find(KeyBindings, key, &EditorKeyBinding::key);
    return found == KeyBindings.end() ? nullptr : &*found;
}

std::vector<ImGuiKeyChord> EditorTerminalReservedKeys() {
    std::vector<ImGuiKeyChord> reserved;
    for (const auto& binding : KeyBindings)
        if (binding.scope == EditorKeyScope::Everywhere || binding.scope == EditorKeyScope::AgentsDrawer)
            reserved.push_back(binding.chord);
    return reserved;
}

std::filesystem::path EditorGuideLocation(const std::filesystem::path& engineDirectory) {
    std::error_code error;
    if (!engineDirectory.empty()) {
        auto local = engineDirectory / "Docs" / "Editor" / "Usage.md";
        if (std::filesystem::is_regular_file(local, error)) return local;
    }
    std::string repository = ENGINE_REPOSITORY;
    if (repository.ends_with(".git")) repository.resize(repository.size() - 4);
    return repository + "/blob/v" ENGINE_VERSION "/Docs/Editor/Usage.md";
}
