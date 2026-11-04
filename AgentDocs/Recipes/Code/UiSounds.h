// [recipe:UiSounds-header]
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
// [/recipe]
