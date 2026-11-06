#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>
#include "Animation/Curve.h"
#include "Fx/FxQuality.h"
#include "Graphics/Draw/ParticleDraw.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"
#include "Utilities/Math/FloatRange.h"
#include "Utilities/Math/Scalar.h"
#include "Utilities/Random/Rng.h"

class Painter;
class ParticleEmitter;

enum class ParticleBlend { Alpha, Additive };

enum class ParticleAreaShape { Point, Disc, Ring, Box, Line };

// Where in the source particles start, around the source's position. Plain constant data, so a theme can hold it.
struct ParticleArea {
    ParticleAreaShape shape = ParticleAreaShape::Point;
    float radius = 0.0f;      // Disc, and a Ring's outer edge
    float innerRadius = 0.0f; // Ring; 0..radius
    Float2 halfSize{};        // Box
    float length = 0.0f;      // Line, centred on the source
    float angle = 0.0f;       // Box and Line turn, radians, clockwise
    // Each particle flies away from the source's position, within the emission's spread, instead of along
    // 'direction'. Not for a Point.
    bool outward = false;

    friend constexpr bool operator==(const ParticleArea&, const ParticleArea&) = default;
};

struct ParticleEmission {
    FloatRange speed;
    FloatRange lifetime;
    FloatRange size;
    FloatRange rotation;    // radians, clockwise
    FloatRange spin;        // radians/second, clockwise
    float direction = 0.0f; // radians, clockwise
    float spread = 2.0f * Pi;
    Float2 acceleration{}; // canvas units/second squared
    float drag = 0.0f;     // Share of velocity lost per second, whatever the frame rate
    ParticleArea area{};
    float inherit = 0.0f; // Share of the source's velocity each particle starts with; 0: none
};

inline constexpr Color4 ParticleWhite{255, 255, 255, 255};

// One colour at one point of a particle's life, 0 (born) to 1 (gone).
struct ParticleColorKey {
    float at = 0.0f;
    Color4 color = ParticleWhite;

    friend constexpr bool operator==(const ParticleColorKey&, const ParticleColorKey&) = default;
};
inline constexpr std::size_t MaxParticleColorKeys = 8;

// A particle's colour through its life: straight lines between the first 'count' keys, in order of 'at'. Before the
// first key it is the first colour, after the last the last. No keys: plain white, so the emitted colour as it is.
struct ParticleColors {
    std::array<ParticleColorKey, MaxParticleColorKeys> keys{};
    int count = 0;

    friend constexpr bool operator==(const ParticleColors&, const ParticleColors&) = default;
};

// The emitted colour, fading out over the last 'fraction' (0..1) of each life.
[[nodiscard]] constexpr ParticleColors ParticleFadeOut(float fraction) {
    constexpr Color4 Gone{255, 255, 255, 0};
    if (!(fraction > 0.0f)) return {};
    if (fraction >= 1.0f) return {.keys = {{{0.0f, ParticleWhite}, {1.0f, Gone}}}, .count = 2};
    return {.keys = {{{0.0f, ParticleWhite}, {1.0f - fraction, ParticleWhite}, {1.0f, Gone}}}, .count = 3};
}

// A particle's size through its life, as a share of its emitted size: from 'start' to 'end' along 'curve'.
struct ParticleSizeCurve {
    float start = 1.0f;
    float end = 1.0f;
    Curve curve = Curve::Linear;

    friend constexpr bool operator==(const ParticleSizeCurve&, const ParticleSizeCurve&) = default;
};

// Plain constant data, so a theme can hold it.
struct ParticleAppearance {
    ParticleShape shape = ParticleShape::Circle;
    ParticleBlend blend = ParticleBlend::Alpha;
    // Multiplies the emitted colour, so one look works in any colour: white to grey darkens it, alpha fades it.
    ParticleColors colors{};
    // The keys' own colours instead, for a look with colours of its own (fire from yellow to red).
    bool ownColors = false;
    ParticleSizeCurve size{};
    float sparkTrailSeconds = 0.0f;
    float sparkThickness = 1.0f; // fraction of initial particle size
    float sparkMinThickness = 0.0f;

    friend constexpr bool operator==(const ParticleAppearance&, const ParticleAppearance&) = default;
};

// The colour of a particle with this look, emitted in 'color', at 'age' (0..1) of its life.
[[nodiscard]] Color4 ParticleColorAt(const ParticleAppearance& appearance, Color4 color, float age);

// Custom drawing instead of the appearance's shape (the blend is already set). Draw only: do not change the system.
// Captured texture handles/references must outlive the emitted particles.
using ParticleCustomDraw = std::function<void(Painter&, const ParticleDrawData&)>;

// Where particles come from: a place and, for ParticleEmission::inherit, how fast it moves. A position alone (a
// Float2, or x and y) is a still source.
struct ParticleSource {
    constexpr ParticleSource(Float2 at = {}, Float2 velocity = {})
        : at(at)
        , velocity(velocity) {}
    constexpr ParticleSource(float x, float y)
        : at{x, y} {}
    Float2 at;
    Float2 velocity;
};

struct ParticleSystemSettings {
    std::size_t capacity;
    // A fixed default: the same emissions look the same on every run, which frame checks rely on.
    std::uint64_t seed = 1;
};

// Its generator is its own, so visual randomness cannot move a simulation's sequence. Particles share their look:
// each emission with the same appearance and movement uses one entry, so a particle holds only its own values.
class ParticleSystem {
public:
    explicit ParticleSystem(const ParticleSystemSettings& settings);
    // What Emit accepts; Emit throws invalid_argument otherwise.
    [[nodiscard]] static bool IsValid(const ParticleEmission& emission, const ParticleAppearance& appearance);
    // Emit makes particlePercent of the requested particles.
    void SetQuality(const FxQuality& quality) { _quality = quality; }
    [[nodiscard]] const FxQuality& Quality() const { return _quality; }
    // 'count' particles in 'color' at once. When full, the oldest particles make room: what just happened matters more
    // than what is fading.
    void Emit(const ParticleSource& source, Color4 color, int count, const ParticleEmission& emission,
              const ParticleAppearance& appearance, const ParticleCustomDraw& draw = {});
    void Update(float deltaSeconds);
    void Clear();
    // Alpha particles first, then additive ones on top.
    void Draw(Painter& paint) const;
    [[nodiscard]] std::size_t ActiveCount() const { return _particles.size(); }
    [[nodiscard]] std::size_t Capacity() const { return _capacity; }

private:
    friend class ParticleEmitter;
    struct Look {
        ParticleAppearance appearance;
        Float2 acceleration;
        float drag = 0.0f;
        ParticleCustomDraw draw;
        std::size_t alive = 0;
    };
    struct Particle {
        Float2 position;
        Float2 velocity;
        float life;
        float duration;
        float size;
        float rotation;
        float spin;
        Color4 color;
        std::uint32_t look;
    };
    std::uint32_t LookFor(const ParticleEmission& emission, const ParticleAppearance& appearance,
                          const ParticleCustomDraw& draw);
    std::size_t MakeRoom(int count);
    // One particle at 'at', already 'age' seconds old.
    void Add(Float2 at, Float2 sourceVelocity, Color4 color, std::uint32_t look, const ParticleEmission& emission,
             float age);
    // For ParticleEmitter: 'count' particles born along the way from 'from' to source.at during the last 'seconds',
    // the first at 'firstAlong' (0..1) of the way and each next 'step' further, each as old as the rest of the way.
    // The quality share is not applied: the emitter applies it to its rate. Validates like Emit.
    void Stream(const ParticleSource& source, Color4 color, int count, const ParticleEmission& emission,
                const ParticleAppearance& appearance, Float2 from, float seconds, float firstAlong, float step);
    void Forget(const Particle& particle);
    void DrawPass(Painter& paint, ParticleBlend blend) const;
    std::size_t _capacity;
    FxQuality _quality;
    std::vector<Particle> _particles;
    std::vector<Look> _looks;
    Rng _rng;
};
