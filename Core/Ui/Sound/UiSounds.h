#pragma once
#include <string_view>
#include "Audio/SoundKey.h"
#include "Ui/Kernel/UiTypes.h"

class SoundPlayer;

// What a game's menus play, as rows of its own sound tables. Every field is optional, and an empty one is silent; the
// engine ships no sounds. For the whole game:
//     settings.uiSounds = {.move = Sfx::UiMove, .confirm = Sfx::UiConfirm, .back = Sfx::UiBack, .deny = Sfx::UiDeny};
// For one Ui, among its looks: `_ui.Looks().Set(UiSounds{.confirm = Sfx::ShopBell});`.
struct UiSounds {
    SoundKey move;    // A cursor moving, by keys or by the mouse
    SoundKey step;    // A value changing: slider, stepper, toggle, tabs. Empty: `move`
    SoundKey confirm; // A press
    SoundKey back;    // A back key a screen answered, or a button with UiSound::Back
    SoundKey deny;    // A press or a step on something disabled
    SoundKey open;    // A modal layer opening
    SoundKey close;   // A modal layer closing
};

// The meaning of what happened: the element's own choice, or what its kind means by default. Never Auto.
[[nodiscard]] UiSound UiSoundOf(const UiEvent& event);
// The row that meaning plays; an empty key for silence.
[[nodiscard]] SoundKey UiSoundKey(const UiSounds& sounds, UiSound sound);
// Throws std::invalid_argument, naming the field and `where`, for a key the player cannot play. Empty keys are fine.
void CheckUiSounds(const UiSounds& sounds, const SoundPlayer& player, std::string_view where);
