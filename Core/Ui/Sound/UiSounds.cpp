#include "Ui/Sound/UiSounds.h"
#include <array>
#include <stdexcept>
#include <string>
#include <utility>
#include "Audio/SoundPlayer.h"

UiSound UiSoundOf(const UiEvent& event) {
    if (event.sound != UiSound::Auto) return event.sound;
    switch (event.kind) {
    case UiEventKind::FocusMoved : return UiSound::Move;
    case UiEventKind::Activated  : return UiSound::Confirm;
    case UiEventKind::Changed    : return UiSound::Step;
    case UiEventKind::Denied     : return UiSound::Deny;
    case UiEventKind::Back       : return UiSound::Back;
    case UiEventKind::LayerOpened: return UiSound::Open;
    case UiEventKind::LayerClosed: return UiSound::Close;
    }
    return UiSound::None;
}

SoundKey UiSoundKey(const UiSounds& sounds, UiSound sound) {
    switch (sound) {
    case UiSound::Move   : return sounds.move;
    case UiSound::Step   : return sounds.step ? sounds.step : sounds.move;
    case UiSound::Confirm: return sounds.confirm;
    case UiSound::Back   : return sounds.back;
    case UiSound::Deny   : return sounds.deny;
    case UiSound::Open   : return sounds.open;
    case UiSound::Close  : return sounds.close;
    case UiSound::Auto   :
    case UiSound::None   : break;
    }
    return {};
}

void CheckUiSounds(const UiSounds& sounds, const SoundPlayer& player, std::string_view where) {
    const std::array<std::pair<const char*, SoundKey>, 7> fields{{{"move", sounds.move},
                                                                  {"step", sounds.step},
                                                                  {"confirm", sounds.confirm},
                                                                  {"back", sounds.back},
                                                                  {"deny", sounds.deny},
                                                                  {"open", sounds.open},
                                                                  {"close", sounds.close}}};
    for (const auto& [name, key] : fields) {
        if (!key || player.Has(key)) continue;
        throw std::invalid_argument(std::string(where) + "." + name + ": row " + std::to_string(key.Index()) +
                                    " of the sound enum " + SoundEnumName(*key.Type()) +
                                    " cannot play; its table must be in GameSettings::sounds, and Count is no row");
    }
}
