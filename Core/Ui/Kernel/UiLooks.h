#pragma once
#include <memory>
#include <typeindex>
#include <unordered_map>

// How things look, one value per widget type. A widget defines a look type whose default values draw it plainly, and
// asks for it with Get; a look set is the list of what differs from those. A game's own widget and look type work the
// same way as the engine's. Change looks between frames: a built frame draws with the looks it was built with. How a Ui
// sounds (UiSounds) is kept here too: it has the same life, a value for the whole game that one Ui can replace.
class UiLooks {
public:
    template <class Look> void Set(Look look) {
        _looks[std::type_index(typeid(Look))] = std::make_shared<const Look>(std::move(look));
    }
    template <class Look> [[nodiscard]] const Look& Get() const {
        const auto found = _looks.find(std::type_index(typeid(Look)));
        if (found != _looks.end()) return *static_cast<const Look*>(found->second.get());
        static const Look defaults{};
        return defaults;
    }

private:
    std::unordered_map<std::type_index, std::shared_ptr<const void>> _looks;
};

// What a new Ui starts with; GameSettings::uiLooks sets it before a game's scenes are created.
[[nodiscard]] const UiLooks& ActiveUiLooks();
void SetActiveUiLooks(UiLooks looks);
