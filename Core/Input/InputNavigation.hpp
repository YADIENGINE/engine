#pragma once
#include <optional>
#include <span>
#include "Input/Input.hpp"

// Borrowed navigation view. The snapshot must outlive the view; do not retain it across input callbacks.
// Player bindings and gameplay actions belong to the consuming game.
class InputNavigation final {
public:
    explicit InputNavigation(const DeviceInput& input);
    [[nodiscard]] const DeviceInput& Device() const { return _input; }
    [[nodiscard]] bool Pressed(InputKey key) const { return _input.Pressed(key); }
    [[nodiscard]] bool Down(InputKey key) const { return _input.Down(key); }
    [[nodiscard]] bool Pressed(Direction direction) const;
    [[nodiscard]] bool Confirm() const;
    [[nodiscard]] bool Back() const;
    [[nodiscard]] bool AnyPressed() const;
    [[nodiscard]] bool AnyHeld() const;
    [[nodiscard]] int Vertical() const;
    [[nodiscard]] int Horizontal() const;
    [[nodiscard]] int TabStep() const;
    // First eligible contact beginning. Use a pointer gesture helper for release activation.
    [[nodiscard]] std::optional<Vec2> PressPoint() const;
    [[nodiscard]] std::optional<Vec2> HoverPoint() const { return _input.mouseHover; }
    [[nodiscard]] std::optional<Vec2> MousePosition() const { return _input.mousePosition; }
    [[nodiscard]] std::span<const Vec2> PressPoints() const { return _input.pressPoints; }
    [[nodiscard]] std::span<const Vec2> HeldPoints() const { return _input.heldPoints; }
    [[nodiscard]] float Wheel() const { return _input.wheel; }

private:
    const DeviceInput& _input;

public:
    InputNavigation(DeviceInput&&) = delete;
    InputNavigation(const DeviceInput&&) = delete;
};
