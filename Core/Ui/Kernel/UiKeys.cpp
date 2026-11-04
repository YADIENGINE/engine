#include "Ui/Kernel/UiKeys.h"

UiKeys UiKeys::All() {
    return {.up = {InputKey::Up, InputKey::W},
            .down = {InputKey::Down, InputKey::S},
            .left = {InputKey::Left, InputKey::A},
            .right = {InputKey::Right, InputKey::D},
            .confirm = {InputKey::Enter, InputKey::Space},
            .back = {InputKey::Escape},
            .previous = {InputKey::Q},
            .next = {InputKey::E}};
}
UiKeys UiKeys::Wasd(Color4 color) {
    return {.up = {InputKey::W},
            .down = {InputKey::S},
            .left = {InputKey::A},
            .right = {InputKey::D},
            .confirm = {InputKey::Space},
            .back = {InputKey::Escape},
            .previous = {InputKey::Q},
            .next = {InputKey::E},
            .color = color};
}
UiKeys UiKeys::Arrows(Color4 color) {
    return {.up = {InputKey::Up},
            .down = {InputKey::Down},
            .left = {InputKey::Left},
            .right = {InputKey::Right},
            .confirm = {InputKey::Enter},
            .back = {InputKey::Escape},
            .previous = {InputKey::PageUp},
            .next = {InputKey::PageDown},
            .color = color};
}
