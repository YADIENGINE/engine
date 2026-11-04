Show the ready options screen with a tab of the game's own, and a scrolling grid under tabs.

APIs: Core/Ui/Kit/UiOptionsScreen.h, Core/Runtime/Game.h, Core/Ui/Widgets/UiTabs.h, Core/Ui/Widgets/UiScroll.h, Core/Ui/Widgets/UiToggle.h, Core/Ui/Widgets/UiStepper.h.

Forward input, build and draw as in `Recipes/UiMenu.md`. Give the Ui `UiSettings::font`.

<!-- recipe:UiOptions-header -->
```cpp
#pragma once
#include "Ui/Kernel/Ui.h"
#include "Ui/Kit/UiOptionsScreen.h"

struct DeviceInput;
class FontFace;
class Painter;

// The ready options screen with a tab of the game's own, and a scrolling list with tabs written by hand.
class RecipeOptions {
public:
    explicit RecipeOptions(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] const UiOptionsValues& Values() const { return _values; }
    [[nodiscard]] bool ShowHints() const { return _showHints; }
    [[nodiscard]] int Difficulty() const { return _difficulty; }
    [[nodiscard]] bool Closed() const { return _closed; }

private:
    void GameRows(Ui& ui);

    Ui _ui;
    UiOptionsValues _values;
    bool _showHints = true;
    int _difficulty = 1;
    bool _closed = false;
};

// A list of levels in a scrolling grid under tabs, on the same widgets.
class RecipeLevels {
public:
    explicit RecipeLevels(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] int Chosen() const { return _chosen; }
    [[nodiscard]] int World() const { return _world; }

private:
    Ui _ui;
    int _world = 0;
    int _chosen = -1;
};
```
<!-- /recipe -->

<!-- recipe:UiOptions -->
```cpp
#include "AgentDocs/Recipes/Code/UiOptions.h"
#include <array>
#include <string>
#include <string_view>
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiPanel.h"
#include "Ui/Widgets/UiScroll.h"
#include "Ui/Widgets/UiStepper.h"
#include "Ui/Widgets/UiTabs.h"
#include "Ui/Widgets/UiToggle.h"

namespace {
constexpr FloatRect Canvas{0.0f, 0.0f, 1280.0f, 720.0f};
constexpr std::array<std::string_view, 3> Difficulties{"Easy", "Normal", "Hard"};
constexpr std::array<std::string_view, 2> Worlds{"Forest", "Caves"};
constexpr int LevelsPerWorld = 30;
constexpr int LevelColumns = 5;
} // namespace

RecipeOptions::RecipeOptions(const FontFace* font)
    : _ui({.canvas = Canvas, .font = font}) {}

void RecipeOptions::Update(float seconds) {
    _ui.Begin(seconds);
    UiOptionsConfig config;
    config.extraTab = "Game";
    config.extraRows = [this](Ui& ui) { GameRows(ui); };
    // In a running game: values = game.UiOptions(), and game.ApplyUiOptions(values) when result.changed.
    const UiOptionsResult result = UiOptionsScreen(_ui, "options", _values, config);
    if (result.closed) _closed = true;
    _ui.End();
}

void RecipeOptions::GameRows(Ui& ui) {
    static_cast<void>(UiToggle(ui, "hints", "Show hints", _showHints, {.tooltip = "Tips during the first levels"}));
    static_cast<void>(UiStepper(ui, "difficulty", "Difficulty", _difficulty, Difficulties));
}

RecipeLevels::RecipeLevels(const FontFace* font)
    : _ui({.canvas = Canvas, .font = font}) {}

void RecipeLevels::Update(float seconds) {
    _ui.Begin(seconds);
    {
        auto panel =
            UiPanel(_ui, "levels", {.width = UiFixed(640.0f), .height = UiFixed(480.0f), .anchor = UiAnchor::Center});
        static_cast<void>(UiTabs(_ui, "worlds", Worlds, _world));
        UiScroll grid(_ui, _world, {.direction = UiDirection::Grid, .columns = LevelColumns, .gap = 8.0f});
        for (int level = 0; level < LevelsPerWorld; ++level) {
            if (UiButton(_ui, level, std::to_string(level + 1), {.width = UiFill()})) _chosen = level;
        }
    }
    _ui.End();
}
```
<!-- /recipe -->

Pitfalls: a `UiScroll` fills the room its parent has, so give the parent a height. Give the scroll a different id per tab, or the open page's scroll position carries over to the next. Values go back to the game with `Game::ApplyUiOptions` only when `result.changed`.

Concept: `Docs/Ui.md#ready-screens`.
