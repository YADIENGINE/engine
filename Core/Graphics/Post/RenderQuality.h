#pragma once

// Glow strength: Full; Reduced (weaker, smaller); Off (glows are not drawn).
enum class GlowQuality { Full, Reduced, Off };
// Bloom resolution: Off; Half (half-resolution source, the default); Full.
enum class BloomQuality { Off, Half, Full };

// Player-selectable rendering cost. Drawing code does not branch on it: Painter applies it (glows), the renderer
// applies the rest. Read it with Painter::Quality() only to skip purely decorative work.
struct RenderQuality {
    GlowQuality glow = GlowQuality::Full;
    BloomQuality bloom = BloomQuality::Half;
    bool backgroundEffects = true; // Decorative background animation (scanlines, drifting grids)
    friend constexpr bool operator==(const RenderQuality&, const RenderQuality&) = default;
};
