#include "Ui/Sound/UiSoundListener.h"
#include "Audio/SoundPlayer.h"
#include "Ui/Kernel/Ui.h"
#include "Ui/Sound/UiSounds.h"

void UiSoundListener::FrameBuilt(const Ui& ui) {
    const UiSounds& sounds = ui.Looks().Get<UiSounds>();
    CheckUiSounds(sounds, _player, "UiSounds of a Ui");
    bool backPlayed = false;
    for (const UiEvent& event : ui.Events()) {
        const UiSound meaning = UiSoundOf(event);
        const SoundKey key = UiSoundKey(sounds, meaning);
        if (!key) continue;
        if (meaning == UiSound::Back) {
            if (backPlayed) continue;
            backPlayed = true;
        }
        const bool repeats = event.kind == UiEventKind::FocusMoved || event.kind == UiEventKind::Changed;
        if (repeats && _player.IsPlaying(key)) continue;
        _player.Play(key);
    }
}
