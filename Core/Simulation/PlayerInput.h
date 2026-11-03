#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

inline constexpr std::size_t MaxPlayerButtons = 32;
inline constexpr std::size_t MaxPlayerAxes = 8;
inline constexpr std::int32_t AxisUnitsPerOne = 1024;

// One player's input for one simulation step, in the game's own actions. Buttons and axes are indexed in the order
// SimulationSettings declares them; a game passes its enum. 'pressed' is separate from 'held', so a tap shorter than a
// step still reaches the simulation. Axes are stored as whole units (1/1024), so live play and a replay read one value.
struct PlayerInput {
    std::uint32_t held = 0;
    std::uint32_t pressed = 0;
    std::array<std::int32_t, MaxPlayerAxes> axes{};

    template <class Button> [[nodiscard]] constexpr bool Held(Button button) const {
        return (held & Bit(button)) != 0U;
    }
    template <class Button> [[nodiscard]] constexpr bool Pressed(Button button) const {
        return (pressed & Bit(button)) != 0U;
    }
    template <class Axis> [[nodiscard]] constexpr float AxisValue(Axis axis) const {
        return static_cast<float>(axes[static_cast<std::size_t>(axis)]) / static_cast<float>(AxisUnitsPerOne);
    }
    // Rounds to the stored precision; the simulation sees the rounded value in live play as well.
    template <class Axis> constexpr void SetAxis(Axis axis, float value) {
        const float units = value * static_cast<float>(AxisUnitsPerOne);
        axes[static_cast<std::size_t>(axis)] = static_cast<std::int32_t>(units + (units < 0.0f ? -0.5f : 0.5f));
    }
    template <class Button> constexpr void SetHeld(Button button, bool down) {
        held = down ? (held | Bit(button)) : (held & ~Bit(button));
    }
    template <class Button> constexpr void SetPressed(Button button) { pressed |= Bit(button); }
    friend constexpr bool operator==(const PlayerInput&, const PlayerInput&) = default;

private:
    template <class Button> [[nodiscard]] static constexpr std::uint32_t Bit(Button button) {
        return std::uint32_t{1} << static_cast<std::uint32_t>(button);
    }
};
