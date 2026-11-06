#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include "Tests/Support/TempDirectory.h"
#include "Editor/Application/EditorCommandState.h"
#include "Editor/Ui/Commands/EditorActions.h"
#include "Editor/Ui/Commands/EditorCommands.h"

namespace {
struct Fixture {
    const std::vector<SceneDefinition> catalog;
    const SceneParameters parameters;
    const EditorDiagnostics diagnostics;
    const std::string status;
    EditorSnapshot snapshot{catalog, nullptr, parameters, {}, diagnostics, {}, EditorPhase::Stopped, status};
    EditorUiActions actions;
    Fixture() { snapshot.capabilities = ResolveEditorCapabilities({}); } // Nothing built, nothing selected
    EditorPreferences preferences;
    int fontSize = 100;
    EditorShellResult result;
    [[nodiscard]] std::vector<EditorAction> List(bool offerProjectList = false, bool drawerOpen = false) {
        return EditorActionList({.snapshot = snapshot,
                                 .actions = actions,
                                 .preferences = preferences,
                                 .fontSize = fontSize,
                                 .offerProjectList = offerProjectList,
                                 .agentsDrawerOpen = drawerOpen,
                                 .result = result});
    }
};
const EditorAction* Find(const std::vector<EditorAction>& list, EditorMenu menu, const std::string& label) {
    const auto found = std::ranges::find_if(
        list, [&](const EditorAction& action) { return action.menu == menu && action.label == label; });
    return found == list.end() ? nullptr : &*found;
}
} // namespace

TEST_CASE("Editor.Actions.EveryItemHasATargetAndShortcutsAreUnique", "[engine-editor]") {
    Fixture fixture;
    GitSnapshot git;
    git.availability = GitAvailability::Ready;
    fixture.snapshot.git = &git;
    const auto list = fixture.List(true);
    std::set<std::string> shortcuts;
    for (const auto& action : list) {
        INFO(EditorMenuTitle(action.menu) << " / " << action.label);
        CHECK(!action.label.empty());
        CHECK((action.run || action.content));
        CHECK((action.enabled || !action.reason.empty())); // Unavailable items say why
        if (*action.shortcut) CHECK(shortcuts.insert(action.shortcut).second);
    }
}

TEST_CASE("Editor.Actions.TasksAreFoundByTheirMenuName", "[engine-editor]") {
    Fixture fixture;
    const auto list = fixture.List();
    CHECK(Find(list, EditorMenu::Build, "Build & Refresh"));
    CHECK(Find(list, EditorMenu::Build, "Build & Play game"));
    CHECK(Find(list, EditorMenu::Build, "Cancel build"));
    CHECK(Find(list, EditorMenu::Scene, "Run scene"));
    CHECK(Find(list, EditorMenu::Help, "Keyboard shortcuts"));
    CHECK(Find(list, EditorMenu::Help, "Editor guide"));
    for (const auto& panel : EditorPanels) {
        const auto* item = Find(list, EditorMenu::View, panel.title);
        REQUIRE(item);
        CHECK(std::string(item->submenu) == "Panels");
    }
    // No item for one scene parameter: scene values live in the Inspector.
    CHECK_FALSE(std::ranges::any_of(list, [](const EditorAction& action) { return action.label == "Language"; }));
    // Nothing to save, so no Save item.
    CHECK_FALSE(std::ranges::any_of(list, [](const EditorAction& action) { return action.label.starts_with("Save"); }));
}

TEST_CASE("Editor.Actions.RunWritesTheirRequest", "[engine-editor]") {
    Fixture fixture;
    auto list = fixture.List();
    Find(list, EditorMenu::Help, "Keyboard shortcuts")->run();
    CHECK(fixture.result.showShortcuts);
    const auto* console = Find(list, EditorMenu::View, "Console");
    REQUIRE(console);
    CHECK(std::string(console->shortcut) == "Ctrl+`");
    console->run(); // A bottom panel: toggled and focused by the workspace, like Ctrl+`
    CHECK(fixture.result.bottomPanel == EditorPanel::Console);
    const bool before = fixture.preferences.Visible(EditorPanel::Inspector);
    Find(list, EditorMenu::View, "Inspector")->run();
    CHECK(fixture.preferences.Visible(EditorPanel::Inspector) != before);
    Find(list, EditorMenu::View, "150%")->run();
    CHECK(fixture.fontSize == 150);
    Find(list, EditorMenu::Agent, "New agent")->run();
    CHECK(fixture.result.agents.add);
}

TEST_CASE("Editor.Actions.GitMenuFollowsAvailability", "[engine-editor]") {
    Fixture fixture;
    const auto gitItems = [&] {
        const auto list = fixture.List();
        return std::ranges::count_if(list, [](const EditorAction& action) { return action.menu == EditorMenu::Git; });
    };
    CHECK(gitItems() == 0); // No project
    GitSnapshot git;
    fixture.snapshot.git = &git;
    git.availability = GitAvailability::NoRepository;
    const auto none = fixture.List();
    CHECK(Find(none, EditorMenu::Git, "Changes")->enabled); // The drawer offers to create a repository
    CHECK(Find(none, EditorMenu::Git, "Fetch")->reason.find("not a Git repository") != std::string::npos);
    git.availability = GitAvailability::Missing;
    const auto list = fixture.List();
    const auto* fetch = Find(list, EditorMenu::Git, "Fetch");
    REQUIRE(fetch);
    CHECK_FALSE(fetch->enabled);
    CHECK(fetch->reason.find("not installed") != std::string::npos);
    git.availability = GitAvailability::Ready;
    git.repositories.push_back({});
    git.repositories.back().branch.behind = 2;
    const auto ready = fixture.List();
    CHECK(Find(ready, EditorMenu::Git, "Pull")->enabled);
    CHECK(Find(ready, EditorMenu::Git, "Push")->reason == "Nothing to push.");
    CHECK(Find(ready, EditorMenu::Git, "Update submodules")->reason == "This project has no submodules.");
}

TEST_CASE("Editor.Actions.GuidePrefersTheEnginesOwnCopy", "[engine-editor]") {
    TempDirectory engine;
    const auto online = EditorGuideLocation(engine.Path()).generic_string();
    CHECK(online.starts_with("https://"));
    CHECK(online.find("/blob/v") != std::string::npos);
    CHECK(online.ends_with("/Docs/Editor/Usage.md"));
    std::filesystem::create_directories(engine.Path() / "Docs" / "Editor");
    std::ofstream(engine.Path() / "Docs" / "Editor" / "Usage.md") << "# Editor\n";
    CHECK(EditorGuideLocation(engine.Path()) == engine.Path() / "Docs" / "Editor" / "Usage.md");
}

TEST_CASE("Editor.Actions.EveryKeyRunsOneAction", "[engine-editor]") {
    Fixture fixture;
    GitSnapshot git;
    git.availability = GitAvailability::Ready;
    fixture.snapshot.git = &git;
    const auto list = fixture.List(false, true);
    std::set<ImGuiKeyChord> chords;
    for (const auto& binding : EditorKeyBindings()) {
        INFO(binding.text);
        CHECK(chords.insert(binding.chord).second);
        CHECK(std::ranges::count(list, binding.key, &EditorAction::key) == 1);
        // A key no menu shows is named in the shortcut list (or folded into the row of its first sibling).
        const auto action = std::ranges::find(list, binding.key, &EditorAction::key);
        if (action != list.end() && !action->inMenu) CHECK(binding.name);
    }
    for (const auto& action : list)
        if (action.key != EditorKey::None) CHECK(std::string(action.shortcut) == EditorKeyInfo(action.key)->text);
    // The commands' own table (toolbar tooltips) names the same keys.
    for (const auto& command : EditorCommands()) {
        if (!command.shortcut) continue;
        const auto binding = std::ranges::find(EditorKeyBindings(), command.shortcut, &EditorKeyBinding::chord);
        REQUIRE(binding != EditorKeyBindings().end());
        CHECK(std::string(binding->text) == command.shortcutLabel);
    }
}

TEST_CASE("Editor.Actions.ConsoleLeavesOnlyEditorWideKeys", "[engine-editor]") {
    const auto reserved = EditorTerminalReservedKeys();
    const auto has = [&](ImGuiKeyChord chord) { return std::ranges::find(reserved, chord) != reserved.end(); };
    CHECK(has(ImGuiMod_Ctrl | ImGuiKey_J));
    CHECK(has(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_P));
    CHECK(has(ImGuiMod_Ctrl | ImGuiKey_5));       // The Agents drawer's
    CHECK_FALSE(has(ImGuiMod_Ctrl | ImGuiKey_P)); // Quick open stays out of a focused terminal
    CHECK_FALSE(has(ImGuiKey_F5));                // Playback keys go to the shell
}

TEST_CASE("Editor.Actions.DrawerKeysNeedTheDrawer", "[engine-editor]") {
    Fixture fixture;
    const auto closed = fixture.List();
    const auto next = std::ranges::find(closed, EditorKey::NextAgent, &EditorAction::key);
    REQUIRE(next != closed.end());
    CHECK_FALSE(next->enabled);
    CHECK_FALSE(next->inMenu);
    CHECK(Find(closed, EditorMenu::Agent, "Show agents"));
    auto open = fixture.List(false, true);
    CHECK(Find(open, EditorMenu::Agent, "Hide agents"));
    std::ranges::find(open, EditorKey::Agent3, &EditorAction::key)->run();
    CHECK(fixture.result.agents.chip == 2u);
    fixture.snapshot.ready = true;
    open = fixture.List(false, true);
    std::ranges::find(open, EditorKey::SendScene, &EditorAction::key)->run(); // To the drawer's chosen agent
    CHECK(fixture.result.agents.sendScene);
    CHECK_FALSE(fixture.result.agent);
}
