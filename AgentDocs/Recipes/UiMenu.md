Write a menu with a dialog and a widget of the game's own.

APIs: Core/Ui/Kernel/Ui.h, Core/Ui/Widgets/UiButton.h, Core/Ui/Widgets/UiLabel.h, Core/Ui/Widgets/UiPanel.h, Core/Ui/Widgets/UiLayer.h.

In a scene: `_ui.Input(input)` in `HandleInput`, `_ui.CancelInput()` in `InputCancelled`, the building in `UpdateReal`, `_ui.Draw(paint)` in `Draw`. `UiSettings::font` comes from `Game::Font()`, or `SceneServices::font` in an editor scene.

<!-- recipe:UiMenu-header -->
```cpp
#pragma once
#include "Ui/Kernel/Ui.h"

struct DeviceInput;
class FontFace;
class Painter;

// A menu and a dialog over it. In a scene the three calls go in HandleInput, UpdateReal and Draw.
class RecipeMenu {
public:
    explicit RecipeMenu(const FontFace* font);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] int Plays() const { return _plays; }
    [[nodiscard]] int TilePresses() const { return _tilePresses; }
    [[nodiscard]] bool AskingToQuit() const { return _askingToQuit; }
    [[nodiscard]] bool Quit() const { return _quit; }

private:
    void Menu();
    void QuitDialog();

    Ui _ui;
    int _plays = 0;
    int _tilePresses = 0;
    bool _askingToQuit = false;
    bool _quit = false;
};
```
<!-- /recipe -->

<!-- recipe:UiMenu -->
```cpp
#include "AgentDocs/Recipes/Code/UiMenu.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiLabel.h"
#include "Ui/Widgets/UiLayer.h"
#include "Ui/Widgets/UiPanel.h"

namespace {
// A widget of the game's own: a function over the same API the engine's widgets use.
[[nodiscard]] bool LevelTile(Ui& ui, UiId id, Color4 color) {
    UiElement tile =
        ui.Element(id, {.width = UiFixed(64.0f), .height = UiFixed(64.0f)}, {.interact = UiInteract::Focus});
    tile.Draw([color](Painter& paint, const UiDrawState& state) {
        paint.Rect(state.rect, Fill(color));
        if (state.focused) paint.Rect(state.rect, Outline(3.0f, CurrentTheme().palette.accent));
    });
    return static_cast<bool>(tile.Activated());
}
} // namespace

RecipeMenu::RecipeMenu(const FontFace* font)
    : _ui({.canvas = {0.0f, 0.0f, 480.0f, 400.0f}, .font = font}) {}

void RecipeMenu::Update(float seconds) {
    _ui.Begin(seconds);
    Menu();
    QuitDialog();
    _ui.End();
}

void RecipeMenu::Menu() {
    auto panel = UiPanel(_ui, "menu", {.alignX = UiAlign::Center, .anchor = UiAnchor::Center});
    UiLabel(_ui, "My Game", {.size = 40.0f});
    if (UiButton(_ui, "play", "Play", {.width = UiFill()})) ++_plays;
    {
        auto tiles = UiRow(_ui, {.gap = 8.0f});
        if (LevelTile(_ui, "first", Rgb(0x60c0ff))) ++_tilePresses;
        if (LevelTile(_ui, "second", Rgb(0xff8060))) ++_tilePresses;
    }
    if (UiButton(_ui, "quit", "Quit", {.width = UiFill()})) _askingToQuit = true;
}

void RecipeMenu::QuitDialog() {
    // The dialog takes the input, and its cursor; closing gives both back.
    const UiLayerSpec centered{.alignX = UiAlign::Center, .alignY = UiAlign::Center};
    if (auto dialog = UiLayer(_ui, "quit dialog", _askingToQuit, centered)) {
        auto panel = UiPanel(_ui, "panel", {.alignX = UiAlign::Center});
        UiLabel(_ui, "Quit the game?");
        auto buttons = UiRow(_ui, {.gap = 8.0f});
        if (UiButton(_ui, "yes", "Quit")) _quit = true;
        if (UiButton(_ui, "no", "Stay") || _ui.Back()) _askingToQuit = false;
    }
}
```
<!-- /recipe -->

Pitfalls: a scope stays open until its variable ends, so two panels side by side each need their own `{ }`. A draw lambda runs after the function has returned: capture by value, and use `ui.Text(...)` for a `string_view` that does not outlive the frame. Give an element a key from the data as its id, not its translated text.

Concept: `Docs/Ui.md#writing-a-screen`.
