#include "Input/Internals/InputReader.h"
#include <raylib.h>
#include <array>
#include "Utilities/EnumTable.h"

namespace {
struct KeyCode {
    InputKey id;
    int code;
};
constexpr std::array InputKeyCodes{KeyCode{InputKey::A, KEY_A},
                                   KeyCode{InputKey::B, KEY_B},
                                   KeyCode{InputKey::C, KEY_C},
                                   KeyCode{InputKey::D, KEY_D},
                                   KeyCode{InputKey::E, KEY_E},
                                   KeyCode{InputKey::F, KEY_F},
                                   KeyCode{InputKey::G, KEY_G},
                                   KeyCode{InputKey::H, KEY_H},
                                   KeyCode{InputKey::I, KEY_I},
                                   KeyCode{InputKey::J, KEY_J},
                                   KeyCode{InputKey::K, KEY_K},
                                   KeyCode{InputKey::L, KEY_L},
                                   KeyCode{InputKey::M, KEY_M},
                                   KeyCode{InputKey::N, KEY_N},
                                   KeyCode{InputKey::O, KEY_O},
                                   KeyCode{InputKey::P, KEY_P},
                                   KeyCode{InputKey::Q, KEY_Q},
                                   KeyCode{InputKey::R, KEY_R},
                                   KeyCode{InputKey::S, KEY_S},
                                   KeyCode{InputKey::T, KEY_T},
                                   KeyCode{InputKey::U, KEY_U},
                                   KeyCode{InputKey::V, KEY_V},
                                   KeyCode{InputKey::W, KEY_W},
                                   KeyCode{InputKey::X, KEY_X},
                                   KeyCode{InputKey::Y, KEY_Y},
                                   KeyCode{InputKey::Z, KEY_Z},

                                   KeyCode{InputKey::Zero, KEY_ZERO},
                                   KeyCode{InputKey::One, KEY_ONE},
                                   KeyCode{InputKey::Two, KEY_TWO},
                                   KeyCode{InputKey::Three, KEY_THREE},
                                   KeyCode{InputKey::Four, KEY_FOUR},
                                   KeyCode{InputKey::Five, KEY_FIVE},
                                   KeyCode{InputKey::Six, KEY_SIX},
                                   KeyCode{InputKey::Seven, KEY_SEVEN},
                                   KeyCode{InputKey::Eight, KEY_EIGHT},
                                   KeyCode{InputKey::Nine, KEY_NINE},

                                   KeyCode{InputKey::Up, KEY_UP},
                                   KeyCode{InputKey::Down, KEY_DOWN},
                                   KeyCode{InputKey::Left, KEY_LEFT},
                                   KeyCode{InputKey::Right, KEY_RIGHT},

                                   KeyCode{InputKey::Enter, KEY_ENTER},
                                   KeyCode{InputKey::Space, KEY_SPACE},
                                   KeyCode{InputKey::Escape, KEY_ESCAPE},

                                   KeyCode{InputKey::LeftShift, KEY_LEFT_SHIFT},
                                   KeyCode{InputKey::RightShift, KEY_RIGHT_SHIFT},
                                   KeyCode{InputKey::LeftControl, KEY_LEFT_CONTROL},
                                   KeyCode{InputKey::RightControl, KEY_RIGHT_CONTROL},
                                   KeyCode{InputKey::LeftAlt, KEY_LEFT_ALT},
                                   KeyCode{InputKey::RightAlt, KEY_RIGHT_ALT},

                                   KeyCode{InputKey::Tab, KEY_TAB},
                                   KeyCode{InputKey::Backspace, KEY_BACKSPACE},
                                   KeyCode{InputKey::Delete, KEY_DELETE},
                                   KeyCode{InputKey::Insert, KEY_INSERT},

                                   KeyCode{InputKey::Home, KEY_HOME},
                                   KeyCode{InputKey::End, KEY_END},
                                   KeyCode{InputKey::PageUp, KEY_PAGE_UP},
                                   KeyCode{InputKey::PageDown, KEY_PAGE_DOWN},

                                   KeyCode{InputKey::Minus, KEY_MINUS},
                                   KeyCode{InputKey::Equal, KEY_EQUAL},
                                   KeyCode{InputKey::Subtract, KEY_KP_SUBTRACT},
                                   KeyCode{InputKey::Add, KEY_KP_ADD},

                                   KeyCode{InputKey::F1, KEY_F1},
                                   KeyCode{InputKey::F2, KEY_F2},
                                   KeyCode{InputKey::F3, KEY_F3},
                                   KeyCode{InputKey::F4, KEY_F4},
                                   KeyCode{InputKey::F5, KEY_F5},
                                   KeyCode{InputKey::F6, KEY_F6},
                                   KeyCode{InputKey::F7, KEY_F7},
                                   KeyCode{InputKey::F8, KEY_F8},
                                   KeyCode{InputKey::F9, KEY_F9},
                                   KeyCode{InputKey::F10, KEY_F10},
                                   KeyCode{InputKey::F11, KEY_F11},
                                   KeyCode{InputKey::F12, KEY_F12}};
static_assert(RowsInIdOrder(InputKeyCodes));
} // namespace

InputSample ReadInputDevices() {
    InputSample sample;
    for (const auto& key : InputKeyCodes) {
        const auto index = static_cast<std::size_t>(key.id);
        sample.keysPressed[index] = IsKeyPressed(key.code);
        sample.keysDown[index] = IsKeyDown(key.code);
    }
    const Vector2 mouse = GetMousePosition();
    sample.mousePosition = {mouse.x, mouse.y};
    const Vector2 delta = GetMouseDelta();
    sample.mouseMoved = delta.x != 0.0f || delta.y != 0.0f;
    sample.mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    sample.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    sample.mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    sample.wheel = GetMouseWheelMove();
    const int touchCount = GetTouchPointCount();
    for (int i = 0; i < touchCount; ++i) {
        const Vector2 point = GetTouchPosition(i);
        sample.touches.push_back({GetTouchPointId(i), {point.x, point.y}});
    }
    return sample;
}
