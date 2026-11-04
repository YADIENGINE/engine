#pragma once
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include "Animation/Tween.h"
#include "Fx/FxQuality.h"
#include "Fx/FxText.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Geometry/FloatRect.h"

class Painter;

struct BannerStyle {
    float duration = 1.0f;
    float fadeSeconds = 0.0f;
    float curtainFadeSeconds = 0.0f;
    Color4 curtain{0, 0, 0, 0};
    Color4 color{255, 255, 255, 255};
    Float2 position{0.5f, 0.5f}; // normalized within supplied bounds
    Float2 offset{};
    FxTextStyle text;
};

struct ToastStyle {
    float duration = 1.0f;
    float queuedDuration = 1.0f;
    float fadeSeconds = 0.0f;
    Float2 position{0.5f, 0.0f};
    Float2 offset{};
    FxTextStyle text;
};

[[nodiscard]] bool IsValid(const BannerStyle& style);
[[nodiscard]] bool IsValid(const ToastStyle& style);

// Text for the player: one banner at a time, and toasts that wait their turn. A new toast shortens the one showing to
// its queued duration; a full queue drops its oldest. Styles come with each call; an invalid one throws
// invalid_argument. Update it with real time: a reader's time does not slow down with the game.
class Announcements {
public:
    explicit Announcements(std::size_t toastCapacity);
    // Flashes off: a banner has no curtain.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    void Banner(const std::string& text, const BannerStyle& style);
    void Toast(const std::string& text, Color4 color, const ToastStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    // Banner, then toast. Text is placed in 'bounds'; the banner curtain covers 'fillBounds'. Opens no clip or
    // transform of its own.
    void Draw(Painter& paint, const FloatRect& bounds, const FloatRect& fillBounds) const;
    void Draw(Painter& paint, const FloatRect& bounds) const { Draw(paint, bounds, bounds); }

private:
    struct BannerEntry {
        std::string text;
        BannerStyle style;
        SecondsTween<float> alpha;
        SecondsTween<float> curtain;
    };
    struct ToastEntry {
        std::string text;
        Color4 color;
        ToastStyle style;
        float life;
    };
    void ShowToast(ToastEntry entry);
    std::size_t _toastCapacity;
    FxQuality _quality;
    std::optional<BannerEntry> _banner;
    std::optional<ToastEntry> _toast;
    std::deque<ToastEntry> _toastQueue;
};
