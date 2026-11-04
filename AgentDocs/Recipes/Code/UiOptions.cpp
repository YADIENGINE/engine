// [recipe:UiOptions]
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
// [/recipe]
