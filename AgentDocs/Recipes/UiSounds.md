# UI sounds recipe

Give a game's menus their sounds once, and let one element or one screen differ.

APIs: Core/Ui/Sound/UiSounds.h, Core/Ui/Kernel/UiTypes.h, Core/Ui/Kernel/UiElement.h, Core/Runtime/GameSettings.h, Core/Audio/SoundRow.h. Concepts: `Docs/Ui.md#sounds`, `Docs/Audio.md#sound-tables`.

<!-- recipe:UiSounds-header -->
```cpp
#pragma once
#include <array>
#include "Audio/SoundRow.h"
#include "Ui/Kernel/Ui.h"
#include "Utilities/EnumTable.h"

struct DeviceInput;
class FontFace;
struct GameSettings;
class Painter;

// A game's own table: the menu's sounds are ordinary rows. Move needs no Skip: the UI never overlaps a cursor move.
enum class RecipeUiSfx { Move, Confirm, Back, Deny, Open, Close, ShopBell, Count };

inline constexpr std::array RecipeUiSfxTable{
    SoundRow{RecipeUiSfx::Move, {"Audio/ui_move.wav"}, {.volume = 0.6f}},
    SoundRow{RecipeUiSfx::Confirm, {"Audio/ui_confirm.wav"}},
    SoundRow{RecipeUiSfx::Back, {"Audio/ui_back.wav"}},
    SoundRow{RecipeUiSfx::Deny, {"Audio/ui_deny.wav"}},
    SoundRow{RecipeUiSfx::Open, {"Audio/ui_open.wav"}},
    SoundRow{RecipeUiSfx::Close, {"Audio/ui_close.wav"}},
    SoundRow{RecipeUiSfx::ShopBell, {"Audio/shop_bell.wav"}},
};
static_assert(RowsInIdOrder(RecipeUiSfxTable));

// Once, for the whole game: every Ui plays these from now on.
void AddRecipeUiSounds(GameSettings& settings);

// A screen needs no sound code. Only what differs from the usual says so.
class RecipeShop {
public:
    explicit RecipeShop(const FontFace* font, bool ownSounds = false);
    void Input(const DeviceInput& input) { _ui.Input(input); }
    void Update(float seconds);
    void Draw(Painter& paint) const { _ui.Draw(paint); }

    [[nodiscard]] int Purchases() const { return _purchases; }
    [[nodiscard]] bool Closed() const { return _closed; }

private:
    Ui _ui;
    int _level = 50;
    int _purchases = 0;
    bool _closed = false;
};
```
<!-- /recipe -->

<!-- recipe:UiSounds -->
```cpp
#include "AgentDocs/Recipes/Code/UiSounds.h"
#include "Runtime/GameSettings.h"
#include "Ui/Sound/UiSounds.h"
#include "Ui/Widgets/UiButton.h"
#include "Ui/Widgets/UiPanel.h"
#include "Ui/Widgets/UiSlider.h"

void AddRecipeUiSounds(GameSettings& settings) {
    settings.sounds = RecipeUiSfxTable;
    settings.uiSounds = {.move = RecipeUiSfx::Move,
                         .confirm = RecipeUiSfx::Confirm,
                         .back = RecipeUiSfx::Back,
                         .deny = RecipeUiSfx::Deny,
                         .open = RecipeUiSfx::Open,
                         .close = RecipeUiSfx::Close};
}

RecipeShop::RecipeShop(const FontFace* font, bool ownSounds)
    : _ui({.canvas = {0.0f, 0.0f, 640.0f, 480.0f}, .font = font}) {
    // One Ui that sounds different from the rest of the game.
    if (ownSounds) _ui.Looks().Set(UiSounds{.move = RecipeUiSfx::Move, .confirm = RecipeUiSfx::ShopBell});
}

void RecipeShop::Update(float seconds) {
    _ui.Begin(seconds);
    {
        auto panel = UiPanel(_ui, "shop", {.width = UiFixed(400.0f), .anchor = UiAnchor::Center});
        // No `step` was given, so a change plays `move`, and never over itself.
        static_cast<void>(UiSlider(_ui, "level", "Level", _level, 0, 100));
        // The game plays its own sound when a purchase goes through, so the button stays silent.
        if (UiButton(_ui, "buy", "Buy", {.width = UiFill(), .sound = UiSound::None})) ++_purchases;
        // It leaves the screen: the same sound as a back key, and one sound if both happen.
        if (UiButton(_ui, "leave", "Leave", {.width = UiFill(), .sound = UiSound::Back}) || _ui.Back()) _closed = true;
    }
    _ui.End();
}
```
<!-- /recipe -->

A widget of the game's own chooses with `UiTraits::sound`, which says what a press plays. If it holds a value, it says when the value changed, and only then: `if (changed) element.ReportChange(step.user);`.

Pitfalls: with no sounds given, menus are silent; the engine ships none. A key whose table is not in `settings.sounds` throws when the game starts, or for one Ui's own sounds on its first frame. A back key sounds only when a screen asks `_ui.Back()` and gets true. Replace one Ui's sounds with `_ui.Looks().Set(...)`, not by assigning a whole look set to `_ui.Looks()`, which would drop them. An editor work scene hears them only if the project's `Editor/Main.cpp` passes the game's settings: `Docs/Editor/GettingStarted.md#editormaincpp`. You cannot hear a sound: say what you checked, and leave whether it sounds right to the developer.
