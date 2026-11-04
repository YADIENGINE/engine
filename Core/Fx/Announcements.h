#pragma once
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include "Fx/FxQuality.h"
#include "Fx/FxText.h"
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRect.h"

class Painter;

struct FlashStyle {
    float duration = 1.0f;
    float opacity = 1.0f;
};

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

class OverlayEffects2D {
public:
    explicit OverlayEffects2D(std::size_t toastCapacity);
    // Flashes off: Flash does nothing (banners and toasts stay).
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    void Flash(Color4 color, const FlashStyle& style);
    void Banner(const std::string& text, const BannerStyle& style);
    void Toast(const std::string& text, Color4 color, const ToastStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    // Flash, then banner, then toast. Text is placed in 'bounds'; the flash and the banner curtain cover
    // 'fillBounds'. Opens no clip or transform of its own.
    void Draw(Painter& paint, const FloatRect& bounds, const FloatRect& fillBounds) const;
    void Draw(Painter& paint, const FloatRect& bounds) const { Draw(paint, bounds, bounds); }

private:
    struct FlashEntry {
        Color4 color;
        FlashStyle style;
        float life;
    };
    struct BannerEntry {
        std::string text;
        BannerStyle style;
        float life;
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
    std::optional<FlashEntry> _flash;
    std::optional<BannerEntry> _banner;
    std::optional<ToastEntry> _toast;
    std::deque<ToastEntry> _toastQueue;
};
