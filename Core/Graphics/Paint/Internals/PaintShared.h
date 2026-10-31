#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string_view>
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Paint/PaintStyle.h"
#include "Graphics/Post/RenderQuality.h"
#include "Utilities/Float2.h"

// Small rules shared by the Painter's drawing files.
inline constexpr float ReducedGlowRadius = 0.7f; // GlowQuality::Reduced
inline constexpr float ReducedGlowStrength = 0.6f;

[[nodiscard]] inline bool Finite(float value) { return std::isfinite(value); }
[[nodiscard]] inline bool Finite(Float2 point) { return std::isfinite(point.x) && std::isfinite(point.y); }

// Widths and radii: negative (or NaN) is a programmer error (DrawList::Reject); false: draw nothing.
[[nodiscard]] inline bool ValidWidth(DrawList& list, float value, std::string_view what) {
    if (value >= 0.0f) return true;
    list.Reject(what);
    return false;
}

[[nodiscard]] inline std::uint8_t EmissionByte(float emission) {
    return static_cast<std::uint8_t>(std::lround(std::clamp(emission, 0.0f, 1.0f) * 255.0f));
}

// The glow a style asks for after the quality setting; nullopt when nothing is drawn.
[[nodiscard]] inline std::optional<GlowStyle> EffectiveGlow(DrawList& list, const GlowStyle& glow,
                                                            const RenderQuality& quality) {
    if (!ValidWidth(list, glow.radius, "Glow radius must not be negative")) return std::nullopt;
    if (glow.radius <= 0.0f || glow.color.a == 0 || glow.strength <= 0.0f) return std::nullopt;
    switch (quality.glow) {
    case GlowQuality::Full: return glow;
    case GlowQuality::Reduced:
        return GlowStyle{glow.radius * ReducedGlowRadius, glow.color, glow.strength * ReducedGlowStrength};
    case GlowQuality::Off: return std::nullopt;
    }
    return glow;
}
