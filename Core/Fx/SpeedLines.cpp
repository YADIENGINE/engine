#include "Fx/SpeedLines.h"
#include <cmath>
#include <stdexcept>
#include "Graphics/Draw/DrawUtils.h"
#include "Graphics/Anim/Easing.h"
#include "Utilities/MathUtils.h"

void DrawSpeedLines(const Rect& bounds, float intensity, Color color, float timeSeconds, const SpeedLineStyle& style) {
    if (style.count < 0 || !std::isfinite(style.rate) || style.rate < 0.0f || !std::isfinite(style.innerRadius.min) ||
        !std::isfinite(style.innerRadius.max) || style.innerRadius.min < 0.0f ||
        style.innerRadius.max < style.innerRadius.min || style.innerRadius.max > 1.0f ||
        !std::isfinite(style.width.min) || !std::isfinite(style.width.max) || style.width.min < 0.0f ||
        style.width.max < style.width.min || !std::isfinite(style.opacity) || style.opacity < 0.0f ||
        style.opacity > 1.0f || !std::isfinite(style.whiten) || style.whiten < 0.0f || style.whiten > 1.0f ||
        !std::isfinite(style.minIntensity) || style.minIntensity < 0.0f) {
        throw std::invalid_argument("Invalid speed line style");
    }
    intensity = Clamp01(intensity);
    if (bounds.width <= 0 || bounds.height <= 0 || intensity <= 0.0f || intensity < style.minIntensity ||
        style.count == 0 || style.width.max <= 0.0f || !std::isfinite(timeSeconds))
        return;
    const Vector2 center = RectCenter(bounds);
    const float outer = std::hypot(bounds.width / 2.0f, bounds.height / 2.0f);
    color = ColorLerp(color, WHITE, style.whiten);
    BeginClip(bounds.x, bounds.y, bounds.width, bounds.height);
    BeginBlendMode(BLEND_ADDITIVE);
    for (int i = 0; i < style.count; ++i) {
        const float cycle = timeSeconds * style.rate + NoiseHash(i * 7 + 1, 1);
        const int seed = i * 131 + static_cast<int>(std::floor(cycle)) * 977;
        const float phase = cycle - std::floor(cycle);
        const float angle = NoiseHash(seed, 1) * 2.0f * PI;
        const float inner =
            outer * (style.innerRadius.max - (style.innerRadius.max - style.innerRadius.min) * NoiseHash(seed + 1, 1));
        const float tip = outer + (inner - outer) * EaseOutCubic(phase * 2.0f);
        const float halfWidth = (style.width.min + (style.width.max - style.width.min) * NoiseHash(seed + 2, 1)) / 2.0f;
        const Color lineColor = Fade(color, style.opacity * intensity * (1.0f - phase));
        const Vector2 direction{std::cos(angle), std::sin(angle)};
        const Vector2 side{-direction.y, direction.x};
        const Vector2 a{center.x + direction.x * outer + side.x * halfWidth,
                        center.y + direction.y * outer + side.y * halfWidth};
        const Vector2 b{center.x + direction.x * outer - side.x * halfWidth,
                        center.y + direction.y * outer - side.y * halfWidth};
        const Vector2 c{center.x + direction.x * tip, center.y + direction.y * tip};
        DrawTriangle(a, b, c, lineColor);
        DrawTriangle(a, c, b, lineColor);
    }
    EndBlendMode();
    EndScissorMode();
}
