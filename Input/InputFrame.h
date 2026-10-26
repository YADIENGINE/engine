#pragma once
#include <algorithm>
#include <optional>
#include "Input/DeviceInput.h"

// Ekranların anlamsal tuş sorguları (onay, geri, eksenler, sekme, tıklama noktası) DeviceInput üzerinde; WASD ve oklar
// ikisi de yön sayılır. Oyuna özel eylemler (oyuncu başına tuşlar) oyunun kendi eşlemesindedir. Header-only,
// raylib'siz.
struct InputFrame : DeviceInput {
    using DeviceInput::Pressed;
    InputFrame() = default;
    explicit InputFrame(const DeviceInput& input)
        : DeviceInput(input) {}
    // Enter ya da Space.
    [[nodiscard]] constexpr bool Confirm() const { return Pressed(InputKey::Enter) || Pressed(InputKey::Space); }
    // Bu frame'de herhangi bir tuş basıldı mı.
    [[nodiscard]] constexpr bool AnyPressed() const {
        return std::any_of(keysPressed.begin(), keysPressed.end(), [](bool pressed) { return pressed; });
    }
    // ESC ya da Android'in geri tuşu (masaüstünde hiç basılmaz).
    [[nodiscard]] constexpr bool Back() const { return Pressed(InputKey::Escape) || Pressed(InputKey::Back); }
    // W/S ya da yukarı/aşağı: -1 / 1; basılmadıysa 0.
    [[nodiscard]] constexpr int Vertical() const {
        if (Pressed(InputKey::W) || Pressed(InputKey::Up)) return -1;
        if (Pressed(InputKey::S) || Pressed(InputKey::Down)) return 1;
        return 0;
    }
    // A/D ya da sol/sağ: -1 / 1; basılmadıysa 0.
    [[nodiscard]] constexpr int Horizontal() const {
        if (Pressed(InputKey::A) || Pressed(InputKey::Left)) return -1;
        if (Pressed(InputKey::D) || Pressed(InputKey::Right)) return 1;
        return 0;
    }
    // Sekme değiştirme: Q önceki (-1), E ya da Tab sonraki (1); basılmadıysa 0.
    [[nodiscard]] constexpr int TabStep() const {
        if (Pressed(InputKey::Q)) return -1;
        if (Pressed(InputKey::E) || Pressed(InputKey::Tab)) return 1;
        return 0;
    }
    // Tıklanan nokta: bu frame'in ilk dokunması (tap), yoksa farenin tıklaması.
    [[nodiscard]] std::optional<Vec2> ClickPoint() const {
        return taps.empty() ? pointer : std::optional{taps.front()};
    }
    // Kaydırılan ızgaralar için: parmak ekrandayken sadece tap sayılır (sürükleme tıklama değildir), yoksa fare.
    [[nodiscard]] std::optional<Vec2> ClickPointNoDrag() const { return touches.empty() ? pointer : ClickPoint(); }
    // 'direction' yönünün tuşlarından (WASD ya da ok) biri basıldı mı.
    [[nodiscard]] constexpr bool Pressed(Direction direction) const {
        switch (direction) {
        case Direction::Up   : return Pressed(InputKey::W) || Pressed(InputKey::Up);
        case Direction::Down : return Pressed(InputKey::S) || Pressed(InputKey::Down);
        case Direction::Left : return Pressed(InputKey::A) || Pressed(InputKey::Left);
        case Direction::Right: return Pressed(InputKey::D) || Pressed(InputKey::Right);
        }
        return false;
    }
};
