#include "Input/InputNavigation.h"
#include <algorithm>

InputNavigation::InputNavigation(const DeviceInput& input)
    : _input(input) {}

bool InputNavigation::Confirm() const { return Pressed(InputKey::Enter) || Pressed(InputKey::Space); }
bool InputNavigation::Back() const { return Pressed(InputKey::Escape); }
bool InputNavigation::AnyPressed() const {
    return std::ranges::any_of(_input.keysPressed, [](bool pressed) { return pressed; });
}
bool InputNavigation::AnyHeld() const {
    return std::ranges::any_of(_input.keysDown, [](bool down) { return down; }) || _input.mouseDown ||
           !_input.touches.empty() || !_input.heldPoints.empty();
}
int InputNavigation::Vertical() const {
    if (Pressed(Direction::Up)) return -1;
    if (Pressed(Direction::Down)) return 1;
    return 0;
}
int InputNavigation::Horizontal() const {
    if (Pressed(Direction::Left)) return -1;
    if (Pressed(Direction::Right)) return 1;
    return 0;
}
int InputNavigation::TabStep() const {
    if (Pressed(InputKey::Q)) return -1;
    if (Pressed(InputKey::E) || Pressed(InputKey::Tab)) return 1;
    return 0;
}
std::optional<Vec2> InputNavigation::PressPoint() const {
    return _input.pressPoints.empty() ? _input.mousePress : std::optional{_input.pressPoints.front()};
}
bool InputNavigation::Pressed(Direction direction) const {
    switch (direction) {
    case Direction::Up   : return Pressed(InputKey::W) || Pressed(InputKey::Up);
    case Direction::Down : return Pressed(InputKey::S) || Pressed(InputKey::Down);
    case Direction::Left : return Pressed(InputKey::A) || Pressed(InputKey::Left);
    case Direction::Right: return Pressed(InputKey::D) || Pressed(InputKey::Right);
    }
    return false;
}
