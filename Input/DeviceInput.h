#pragma once
#include <array>
#include <cstddef>
#include <optional>
#include <vector>
#include "Utilities/Vec2.h"

// clang-format off
enum class InputKey { W, S, A, D, Up, Down, Left, Right, Enter, Space, Escape, Back, Q, R, One, Two, Three, Four, L, K, Minus, Equal, Subtract, Add, B, C, E, F, G, H, I, J, M, N, O, P, T, U, V, X, Y, Z, Zero, Five, Six, Seven, Eight, Nine, LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt, Tab, Backspace, Delete, Insert, Home, End, PageUp, PageDown, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, Count };
// clang-format on

struct InputTouch {
    int id = 0;
    std::optional<Vec2> position; // Canvas dışındaki dokunuşun kimliği de korunur.
};

// Tek frame'in cihaz verisi (InputReader doldurur); ekranlar basma/bırakma ve jest kurallarını kendileri yorumlar.
// Saf veri: raylib bilmez, testlerde elle kurulabilir.
struct DeviceInput {
    // Temas/odak kaybı: bu snapshot'taki eylemler işlenmez, bekleyen gesture'lar iptal edilir.
    bool cancelled = false;
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysPressed{};
    std::array<bool, static_cast<std::size_t>(InputKey::Count)> keysDown{};
    bool shiftHeld = false;
    std::optional<Vec2> mouse;
    bool mousePressed = false;
    bool mouseReleased = false;
    bool mouseDown = false;
    std::optional<Vec2> pointer; // Sol tuş bu frame'de basıldıysa canvas konumu.
    std::optional<Vec2> hover;   // Fare bu frame'de hareket ettiyse canvas konumu.
    float wheel = 0.0f;          // Fare tekerleğinin bu frame'deki dönüşü (artı: yukarı).
    std::vector<InputTouch> touches;
    // Bu frame'de yeni inen parmakların canvas konumları; canvas'ta dokunuş yoksa farenin tıklaması. Basılı tutulan
    // parmak yeniden sayılmaz; aynı anda farklı parmaklarla bağımsız eylemler yapılabilir.
    std::vector<Vec2> taps;
    // Şu an basılı parmakların canvas konumları; canvas'ta dokunuş yoksa basılı farenin konumu.
    std::vector<Vec2> held;

    [[nodiscard]] constexpr bool Pressed(InputKey key) const { return keysPressed[static_cast<std::size_t>(key)]; }
    [[nodiscard]] constexpr bool Down(InputKey key) const { return keysDown[static_cast<std::size_t>(key)]; }
};
