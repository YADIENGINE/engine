#pragma once
#include <raylib.h>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include "Graphics/Draw/TextStyle.h"
#include "Utilities/Rect.h"

class TextRenderer;

struct FlashStyle {
    float duration = 1.0f;
    float opacity = 1.0f;
};

struct BannerStyle {
    float duration = 1.0f;
    float fadeSeconds = 0.0f;
    float curtainFadeSeconds = 0.0f;
    Color curtain = BLANK;
    Color color = WHITE;
    Vector2 position{0.5f, 0.5f}; // normalized within supplied bounds
    Vector2 offset{};
    TextStyle text;
};

struct ToastStyle {
    float duration = 1.0f;
    float queuedDuration = 1.0f;
    float fadeSeconds = 0.0f;
    Vector2 position{0.5f, 0.0f};
    Vector2 offset{};
    TextStyle text;
};

class OverlayEffects2D {
public:
    explicit OverlayEffects2D(std::size_t toastCapacity);
    void Flash(Color color, const FlashStyle& style);
    void Banner(const std::string& text, const BannerStyle& style);
    void Toast(const std::string& text, Color color, const ToastStyle& style);
    void Update(float deltaSeconds);
    void Clear();
    // Flash, then banner, then toast. Does not open a clipping or camera scope.
    void Draw(const TextRenderer& textRenderer, const Rect& bounds) const;
    // Text uses bounds; flash and banner curtain cover fillBounds.
    void Draw(const TextRenderer& textRenderer, const Rect& bounds, const Rect& fillBounds) const;

private:
    struct FlashEntry {
        Color color;
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
        Color color;
        ToastStyle style;
        float life;
    };
    void ShowToast(ToastEntry entry);
    std::size_t _toastCapacity;
    std::optional<FlashEntry> _flash;
    std::optional<BannerEntry> _banner;
    std::optional<ToastEntry> _toast;
    std::deque<ToastEntry> _toastQueue;
};
