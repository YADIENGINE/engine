#pragma once
#include "Graphics/Style/NeonArcadePalette.h"
#include "Graphics/Style/Theme.h"
#include "Utilities/Math/Scalar.h"

// Ready-made neon arcade theme: dark purple colours, a scrolling grid, fast and bright effects. Its widgets are in
// Ui/Looks/Neon/NeonLooks.h. Start a new theme by copying this and changing its fields.
inline constexpr TextShadow NeonArcadeTextShadow{.offset = {2.0f, 2.0f}, .color = {0, 0, 0, 153}};
inline constexpr float NeonArcadeVignetteSize = 102.0f;
inline constexpr Color4 NeonArcadeWhite{255, 255, 255, 255};

// Effects: fast, bright, short-lived particles; strong but quickly fading camera punches.
inline constexpr ParticleEmission NeonArcadeEmission{.speed = {60.0f, 220.0f},
                                                     .lifetime = {0.35f, 0.7f},
                                                     .size = {2.0f, 4.5f},
                                                     .rotation = {0.0f, Radians(360.0f)},
                                                     .spin = {Radians(-540.0f), Radians(540.0f)},
                                                     .drag = 3.0f};

// One burst layer: the shared emission at a speed, under a gravity (negative: rising).
[[nodiscard]] constexpr ParticleLayer NeonArcadeLayer(float share, ParticleShape shape, float speedScale, float gravity,
                                                      ParticleBlend blend) {
    ParticleLayer layer{.share = share, .emission = NeonArcadeEmission};
    layer.emission.speed = {NeonArcadeEmission.speed.min * speedScale, NeonArcadeEmission.speed.max * speedScale};
    layer.emission.acceleration = {0.0f, gravity};
    layer.appearance = {.shape = shape,
                        .blend = blend,
                        .endSizeScale = 0.5f,
                        .sizeScale = shape == ParticleShape::Star ? 1.8f : shape == ParticleShape::Pixel ? 1.4f : 1.0f,
                        .sparkTrailSeconds = 0.045f,
                        .sparkThickness = 0.7f,
                        .sparkMinThickness = 1.0f};
    return layer;
}
[[nodiscard]] constexpr ParticleLayer NeonArcadeDash() {
    ParticleLayer layer = NeonArcadeLayer(1.0f, ParticleShape::Spark, 1.4f, 0.0f, ParticleBlend::Additive);
    layer.emission.spread = 0.7f;
    return layer;
}

inline constexpr float NeonArcadeSparkSpeed = 1.7f;
inline constexpr float NeonArcadeSparkShare = 0.35f;

inline constexpr ParticleRecipes NeonArcadeParticles{
    .soft = {NeonArcadeLayer(1.0f, ParticleShape::Circle, 1.0f, 0.0f, ParticleBlend::Alpha),
             NeonArcadeLayer(NeonArcadeSparkShare, ParticleShape::Spark, NeonArcadeSparkSpeed, 0.0f,
                             ParticleBlend::Additive)},
    .spark = {NeonArcadeLayer(1.0f, ParticleShape::Spark, NeonArcadeSparkSpeed, 0.0f, ParticleBlend::Additive),
              ParticleLayer{}},
    .sparkle = {NeonArcadeLayer(1.0f, ParticleShape::Star, 0.6f, -50.0f, ParticleBlend::Additive),
                NeonArcadeLayer(0.5f, ParticleShape::Circle, 1.0f, 0.0f, ParticleBlend::Alpha)},
    .debris = {NeonArcadeLayer(1.0f, ParticleShape::Square, 1.3f, 650.0f, ParticleBlend::Alpha),
               NeonArcadeLayer(NeonArcadeSparkShare, ParticleShape::Spark, NeonArcadeSparkSpeed, 0.0f,
                               ParticleBlend::Additive)},
    .pixel = {NeonArcadeLayer(1.0f, ParticleShape::Pixel, 1.0f, 450.0f, ParticleBlend::Alpha), ParticleLayer{}},
    .confetti = {NeonArcadeLayer(1.0f, ParticleShape::Square, 0.8f, 140.0f, ParticleBlend::Alpha), ParticleLayer{}},
    .trail = NeonArcadeLayer(1.0f, ParticleShape::Circle, 0.15f, 0.0f, ParticleBlend::Additive),
    .trailCount = 3,
    .dash = NeonArcadeDash(),
    .dashCount = 3,
    .steam = {.share = 1.0f,
              .emission = {.speed = {12.0f, 42.0f},
                           .lifetime = {0.35f, 0.65f},
                           .size = {1.5f, 3.0f},
                           .direction = -Pi * 0.5f,
                           .spread = Pi * 0.65f,
                           .acceleration = {0.0f, -18.0f},
                           .drag = 1.5f},
              .appearance = {.blend = ParticleBlend::Additive, .endSizeScale = 2.5f}},
    .steamCount = 2,
    .steamWhiten = 0.65f,
    .steamOpacity = 0.6f};

inline constexpr FxStyle NeonArcadeFx{
    .particleCapacity = 600,
    .toastCapacity = 3,
    .particles = NeonArcadeParticles,
    .camera = {.shakeOffset = 18.0f,
               .shakeAngle = Radians(1.0f),
               .shakeSpeed = 24.0f,
               .shakeDecay = 1.8f,
               .punchDecay = 8.0f,
               .kickImpulse = 110.0f,
               .kickStiffness = 400.0f,
               .kickDamping = 22.0f,
               .maxStep = 1.0f / 30.0f,
               .motionBlendSpeed = 6.0f,
               .motionLeadSpeed = 5.0f,
               .motionLeadDistance = 12.0f,
               .motionZoomOut = 0.025f},
    .text = {.duration = 0.9f,
             .velocity = {0.0f, -45.0f},
             .fadeSeconds = 0.45f,
             .text = {.size = 22.0f, .anchor = {0.5f, 1.0f}, .shadow = NeonArcadeTextShadow}},
    .textPopSeconds = 0.25f,
    .ring = {.duration = 0.5f, .endRadius = 62.5f, .thickness = 4.0f, .fadeSeconds = 0.5f},
    .flash = {.duration = 0.35f, .opacity = 140.0f / 255.0f},
    .banner = {.duration = 1.6f,
               .fadeSeconds = 0.6f,
               .curtainFadeSeconds = 0.6f,
               .curtain = {0, 0, 0, 255},
               .color = NeonArcadePalette.text,
               .text = {.size = 80.0f, .anchor = {0.5f, 0.5f}, .shadow = NeonArcadeTextShadow}},
    .toast = {.duration = 1.6f,
              .queuedDuration = 0.9f,
              .fadeSeconds = 0.4f,
              .offset = {0.0f, 40.0f},
              .text = {.size = 34.0f, .shadow = NeonArcadeTextShadow}},
    .speedLines = {.count = 40,
                   .rate = 2.2f,
                   .innerRadius = {0.6f, 0.9f},
                   .width = {2.0f, 7.0f},
                   .opacity = 0.45f,
                   .whiten = 0.55f,
                   .minIntensity = 0.01f},
    // Pulse: the area glows slightly (additive), edges beat twice like a heartbeat.
    .pulse = {.fadeSpeed = 1.5f,    // Intensity change per second
              .period = 0.85f,      // Duration of one double beat (s)
              .secondBeatAt = 0.2f, // Second beat delay (s)
              .secondBeatScale = 0.6f,
              .decay = 11.0f, // Beat decay rate
              .tintAlpha = 0.05f,
              .tintBeatAlpha = 0.06f,
              .vignetteSize = NeonArcadeVignetteSize,
              .vignetteAlpha = 0.18f,
              .vignetteBeatAlpha = 0.32f},
    // Shockwave: expands to the area diagonal (or supplied reach), followed by a second, thin ring.
    .shockwave = {.duration = 0.9f,
                  .trailDelay = 0.12f,
                  .width = 14.0f,
                  .trailWidth = 0.4f,
                  .opacity = 0.9f,
                  .trailOpacity = 0.5f,
                  .rimWidth = 2.0f,
                  .rim = NeonArcadeWhite},
    // Glitch: red / cyan strips, dark tears and static dots redistributed each frame.
    .glitch = {.fps = 24.0f,
               .bands = 9,
               .minHeight = 3,
               .maxHeight = 26,
               .tearMinHeight = 3,
               .tearMaxHeight = 29,
               .shift = 60.0f, // px; scales with intensity
               .bandAlpha = 0.35f,
               .tears = 4,
               .tearAlpha = 0.75f,
               .noise = 70,
               .noiseSize = 4,
               .noiseAlpha = 0.8f,
               .first = {255, 40, 90, 255},
               .second = {40, 240, 255, 255},
               .tear = {6, 4, 12, 255},
               .highlight = NeonArcadeWhite},
    .slowMotion = {.fadeStart = 0.7f,
                   .tintAlpha = 0.04f,
                   .vignetteSize = 100.0f,
                   .vignetteAlpha = 0.3f,
                   .lines = {.count = 24,
                             .rate = 1.0f,
                             .innerRadius = {0.65f, 0.9f},
                             .width = {1.0f, 4.0f},
                             .opacity = 0.3f,
                             .whiten = 0.45f,
                             .minIntensity = 0.01f}},
    .vignetteSize = NeonArcadeVignetteSize,
    .vignetteColor = {0, 0, 0, 110}};

// Backgrounds: purple grid scrolling downward; dark vignette in menus.
inline constexpr BackdropTheme NeonArcadeBackdrop{
    .menu = {.gridCell = 50,
             .gridScrollSpeed = 18.0f,
             .gridColor = {70, 45, 140, 55},
             .vignetteSize = 220,
             .vignetteColor = {0, 0, 0, 170},
             .scanlineAlpha = 0.12f},
    .play = {.gridCell = 50, .gridScrollSpeed = 10.0f, .gridColor = {70, 45, 140, 40}, .scanlineAlpha = 0.07f},
    .gallery = {.gridCell = 50, .gridScrollSpeed = 10.0f, .gridColor = {70, 45, 140, 40}, .scanlineAlpha = 0.06f},
    .scanlines = {.color = {0, 0, 0, 255}, .spacing = 4.0f, .thickness = 1.0f}};

inline constexpr Theme NeonArcadeTheme{
    .palette = NeonArcadePalette,
    .fx = NeonArcadeFx,
    .backdrop = NeonArcadeBackdrop};
