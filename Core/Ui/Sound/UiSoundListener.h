#pragma once
#include "Ui/Kernel/UiListener.h"

class SoundPlayer;

// Plays what each Ui's frame says, from that Ui's UiSounds. Everything it decides runs with no device: the player's
// SoundOutput is the only part that reaches one, and a test gives it a fake.
//
// A cursor move and a change never overlap themselves, whatever their row's whenPlaying says, so a held key is not a
// machine gun. Everything else plays every time, except that a frame plays the back sound once: a back button and a
// back key can both fire in it.
class UiSoundListener final : public UiListener {
public:
    explicit UiSoundListener(SoundPlayer& player)
        : _player(player) {}
    // Throws std::invalid_argument when a Ui's own UiSounds has a key that cannot play, on the first frame it shows.
    void FrameBuilt(const Ui& ui) override;

private:
    SoundPlayer& _player;
};
