#include "Fx/SpeedLines.h"
#include <cmath>
#include <stdexcept>
#include "Graphics/Anim/Easing.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/MathUtils.h"

void DrawSpeedLines(Painter& paint, const FloatRect& bounds, float intensity, Color4 color, float timeSeconds,
                    const SpeedLineStyle& style) {
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
    if (bounds.Empty() || intensity <= 0.0f || intensity < style.minIntensity || style.count == 0 ||
        style.width.max <= 0.0f || !std::isfinite(timeSeconds))
        return;
    const Float2 center = bounds.Center();
    const float outer = std::hypot(bounds.width * 0.5f, bounds.height * 0.5f);
    color = Mix(color, Color4{255, 255, 255, color.a}, style.whiten);
    auto clip = paint.Clip(bounds);
    auto light = paint.Blending(Blend::Add);
    for (int i = 0; i < style.count; ++i) {
        const float cycle = timeSeconds * style.rate + NoiseHash(i * 7 + 1, 1);
        const int seed = i * 131 + static_cast<int>(std::floor(cycle)) * 977;
        const float phase = cycle - std::floor(cycle);
        const Float2 direction = Float2::FromAngle(NoiseHash(seed, 1) * 2.0f * Pi);
        const float inner =
            outer * (style.innerRadius.max - (style.innerRadius.max - style.innerRadius.min) * NoiseHash(seed + 1, 1));
        const float tip = outer + (inner - outer) * EaseOutCubic(phase * 2.0f);
        const float halfWidth = (style.width.min + (style.width.max - style.width.min) * NoiseHash(seed + 2, 1)) / 2.0f;
        const Float2 side = Perpendicular(direction) * halfWidth;
        const Float2 base = center + direction * outer;
        paint.Triangle(base + side, base - side, center + direction * tip,
                       Fill(WithAlpha(color, style.opacity * intensity * (1.0f - phase))));
    }
}
