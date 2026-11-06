#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include "Tests/Support/TempDirectory.h"
#include "Editor/Application/EditorCommandState.h"
#include "Editor/Ui/Commands/EditorActions.h"

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
    [[nodiscard]] std::vector<EditorAction> List(bool offerProjectList = false) {
        return EditorActionList({.snapshot = snapshot,
                                 .actions = actions,
                                 .preferences = preferences,
                                 .fontSize = fontSize,
                                 .offerProjectList = offerProjectList,
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
    const bool before = fixture.preferences.Visible(EditorPanel::Console);
    console->run();
    CHECK(fixture.preferences.Visible(EditorPanel::Console) != before);
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
    CHECK(gitItems() == 0); // The Git bar offers to create one
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
