#include "Input/InputReader.h"
#include <raylib.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>
#include <vector>
#include "Graphics/Render/Renderer.h"

namespace {
constexpr std::array InputKeyCodes{KEY_W,
                                   KEY_S,
                                   KEY_A,
                                   KEY_D,
                                   KEY_UP,
                                   KEY_DOWN,
                                   KEY_LEFT,
                                   KEY_RIGHT,
                                   KEY_ENTER,
                                   KEY_SPACE,
                                   KEY_ESCAPE,
                                   KEY_BACK,
                                   KEY_Q,
                                   KEY_R,
                                   KEY_ONE,
                                   KEY_TWO,
                                   KEY_THREE,
                                   KEY_FOUR,
                                   KEY_L,
                                   KEY_K,
                                   KEY_MINUS,
                                   KEY_EQUAL,
                                   KEY_KP_SUBTRACT,
                                   KEY_KP_ADD,
                                   KEY_B,
                                   KEY_C,
                                   KEY_E,
                                   KEY_F,
                                   KEY_G,
                                   KEY_H,
                                   KEY_I,
                                   KEY_J,
                                   KEY_M,
                                   KEY_N,
                                   KEY_O,
                                   KEY_P,
                                   KEY_T,
                                   KEY_U,
                                   KEY_V,
                                   KEY_X,
                                   KEY_Y,
                                   KEY_Z,
                                   KEY_ZERO,
                                   KEY_FIVE,
                                   KEY_SIX,
                                   KEY_SEVEN,
                                   KEY_EIGHT,
                                   KEY_NINE,
                                   KEY_LEFT_SHIFT,
                                   KEY_RIGHT_SHIFT,
                                   KEY_LEFT_CONTROL,
                                   KEY_RIGHT_CONTROL,
                                   KEY_LEFT_ALT,
                                   KEY_RIGHT_ALT,
                                   KEY_TAB,
                                   KEY_BACKSPACE,
                                   KEY_DELETE,
                                   KEY_INSERT,
                                   KEY_HOME,
                                   KEY_END,
                                   KEY_PAGE_UP,
                                   KEY_PAGE_DOWN,
                                   KEY_F1,
                                   KEY_F2,
                                   KEY_F3,
                                   KEY_F4,
                                   KEY_F5,
                                   KEY_F6,
                                   KEY_F7,
                                   KEY_F8,
                                   KEY_F9,
                                   KEY_F10,
                                   KEY_F11,
                                   KEY_F12};
static_assert(InputKeyCodes.size() == static_cast<std::size_t>(InputKey::Count));
} // namespace

DeviceInput InputReader::Read(const Renderer& renderer) {
    return Read([&renderer](Vec2 point) { return renderer.ToCanvas(point); });
}

DeviceInput InputReader::Read() {
    return Read([](Vec2 point) { return std::optional{point}; });
}

DeviceInput InputReader::Read(const std::function<std::optional<Vec2>(Vec2)>& mapPoint) {
    DeviceInput input;
    for (std::size_t i = 0; i < InputKeyCodes.size(); ++i) {
        input.keysPressed[i] = IsKeyPressed(InputKeyCodes[i]);
        input.keysDown[i] = IsKeyDown(InputKeyCodes[i]);
    }
    input.shiftHeld = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    const auto toCanvas = [&mapPoint](Vector2 position) {
        return mapPoint({static_cast<int>(position.x), static_cast<int>(position.y)});
    };
    input.mouse = toCanvas(GetMousePosition());
    input.mousePressed = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    input.mouseReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
    input.mouseDown = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    if (input.mousePressed) input.pointer = input.mouse;
    const Vector2 delta = GetMouseDelta();
    if (delta.x != 0.0f || delta.y != 0.0f) input.hover = input.mouse;
    input.wheel = GetMouseWheelMove();

    std::vector<int> touchIds;
    const int touchCount = GetTouchPointCount();
    for (int i = 0; i < touchCount; ++i) {
        const InputTouch touch{GetTouchPointId(i), toCanvas(GetTouchPosition(i))};
        input.touches.push_back(touch);
        touchIds.push_back(touch.id);
        if (!touch.position) continue;
        input.held.push_back(*touch.position);
        if (std::find(_touchIds.begin(), _touchIds.end(), touch.id) == _touchIds.end()) {
            input.taps.push_back(*touch.position);
        }
    }
    _touchIds = std::move(touchIds);

    // Android'de dokunuş fareyi de taklit eder; fare sadece canvas'ta geçerli dokunuş yokken sayılır.
    if (input.held.empty()) {
        if (input.pointer) input.taps.push_back(*input.pointer);
        if (input.mouseDown && input.mouse) input.held.push_back(*input.mouse);
    }
    return input;
}
