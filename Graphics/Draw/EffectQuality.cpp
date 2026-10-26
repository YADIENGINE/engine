#include "Graphics/Draw/EffectQuality.h"
#include <algorithm>

namespace {
constexpr float ReducedShake = 0.4f;

EffectQuality& Quality() {
    static EffectQuality quality;
    return quality;
}
} // namespace

const EffectQuality& CurrentEffectQuality() { return Quality(); }

void SetEffectQuality(const EffectQuality& quality) {
    Quality() = quality;
    Quality().particlePercent = std::clamp(quality.particlePercent, 0, 100);
}

float ShakeScale() {
    switch (Quality().shake) {
    case EffectLevel::Full   : return 1.0f;
    case EffectLevel::Reduced: return ReducedShake;
    case EffectLevel::Off    : return 0.0f;
    }
    return 1.0f;
}

int ScaledParticleCount(int count) {
    if (count <= 0 || Quality().particlePercent <= 0) return 0;
    return std::max(1, count * Quality().particlePercent / 100);
}
