// [recipe:UiSounds]
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
// [/recipe]
