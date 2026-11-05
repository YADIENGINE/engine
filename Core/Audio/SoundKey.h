#pragma once
#include <cstddef>
#include <type_traits>
#include <typeinfo>

// An enum a sound table can use: it ends with Count, as RowsInIdOrder needs. SoundId has none, so a raw catalog ID
// cannot become a key by accident.
template <class Enum>
concept SoundEnum = std::is_enum_v<Enum> && requires { Enum::Count; };

// One sound of any game's table, for a system that must store it without knowing the game's enum. It is made
// implicitly from the enum value. An empty key means "no sound"; SoundPlayer::Has answers whether a key can play.
class SoundKey {
public:
    SoundKey() = default;
    template <SoundEnum Enum>
    SoundKey(Enum value) // Implicit on purpose: `.move = Sfx::UiMove`
        : _type(&typeid(Enum))
        , _index(static_cast<std::size_t>(value)) {}

    [[nodiscard]] explicit operator bool() const { return _type != nullptr; }
    // The enum's type; null for an empty key.
    [[nodiscard]] const std::type_info* Type() const { return _type; }
    [[nodiscard]] std::size_t Index() const { return _index; }
    friend bool operator==(const SoundKey& a, const SoundKey& b) {
        if (!a._type || !b._type) return a._type == b._type;
        return *a._type == *b._type && a._index == b._index;
    }

private:
    const std::type_info* _type = nullptr;
    std::size_t _index = 0;
};
