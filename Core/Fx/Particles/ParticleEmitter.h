#pragma once
#include <optional>
#include "Fx/Particles/ParticleSystem.h"
#include "Utilities/Color4.h"
#include "Utilities/Geometry/Float2.h"

// How a ParticleEmitter makes particles. Plain constant data, so a theme can hold it.
struct ParticleEmitterStyle {
    ParticleEmission emission;
    ParticleAppearance appearance;
    float perSecond = 0.0f; // Particles each second, moving or not
    float perUnit = 0.0f;   // Particles each canvas unit the source travels
};

// What ParticleEmitter accepts. Both rates are finite and not negative; when either is above 0, the emission and
// appearance are valid too.
[[nodiscard]] bool IsValid(const ParticleEmitterStyle& style);

// A steady stream of particles from something that keeps going: the trail behind a moving object, smoke from a
// chimney, a torch's flame. It remembers where its source was and the share of a particle still owed, so it belongs
// to that object, like a Tween: one each, and it goes when the object goes. FxStage::Emit runs it on the stage's
// particles. For something that happens once, use a burst instead.
class ParticleEmitter {
public:
    // Throws invalid_argument for an invalid style.
    explicit ParticleEmitter(const ParticleEmitterStyle& style);

    // Each frame, with the seconds since the last call on the clock that moves the source (usually game time).
    // Particles are born along the way since the last call, each as old as the rest of the frame, so a fast source
    // leaves an even line at any frame rate. That way also gives the source's velocity for ParticleEmission::inherit.
    // The system's quality scales the rates.
    void Emit(ParticleSystem& particles, Float2 at, Color4 color, float seconds);
    // The next Emit starts where the source is and does not fill the way from the last one: after a respawn or a
    // teleport, and after frames without Emit (the stream was off or hidden), which would otherwise draw a line.
    void Jump() { _last.reset(); }
    // A new look or rate (stronger smoke, say); the place and the share owed stay.
    void SetStyle(const ParticleEmitterStyle& style);
    [[nodiscard]] const ParticleEmitterStyle& Style() const { return _style; }

private:
    ParticleEmitterStyle _style;
    std::optional<Float2> _last;
    double _owed = 0.0;
};
