#include "Graphics/Atmosphere/GroundRipples.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Atmosphere/Internals/AtmosphereView.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"

namespace {
constexpr float DensityArea = 100.0f * 100.0f;
constexpr unsigned MaxRipples = 8000U;
constexpr float ReducedShare = 0.5f;

float Share(unsigned seed) { return static_cast<float>(Hash(seed)) / 4294967296.0f; }
} // namespace

bool IsValid(const RippleLook& look) {
    return std::isfinite(look.density) && look.density >= 0.0f && std::isfinite(look.seconds) && look.seconds > 0.0f &&
           std::isfinite(look.radius) && look.radius > 0.0f && std::isfinite(look.squash) && look.squash > 0.0f &&
           look.squash <= 1.0f && std::isfinite(look.width) && look.width > 0.0f;
}

GroundRipples::GroundRipples(const RippleLook& look, std::uint64_t seed)
    : _look(look)
    , _seed(Hash(static_cast<unsigned>(seed))) {
    if (!IsValid(look)) throw std::invalid_argument("Invalid ripple look");
    const float wanted = look.density * AtmosphereTile.x * AtmosphereTile.y / DensityArea;
    _count = std::min(static_cast<unsigned>(std::ceil(wanted)), MaxRipples);
}

void GroundRipples::Update(float seconds) {
    if (seconds > 0.0f && std::isfinite(seconds)) _time += seconds;
}

void GroundRipples::Draw(Painter& paint, float amount, const CameraView& view) const {
    if (paint.CurrentTransform() != Affine2::Identity())
        return paint.Reject("GroundRipples::Draw must be called outside paint.Camera and paint.Transform");
    if (!(amount > 0.0f) || _count == 0U || _look.color.a == 0) return;
    const CameraView layer = AtmosphereLayerView(paint, view, 1.0f);
    const FloatRect seen = AtmosphereSeenArea(layer);
    const float share = std::min(amount, 1.0f) * (paint.Quality().backgroundEffects ? 1.0f : ReducedShare) *
                        AtmosphereThinning(seen);
    const auto showing = static_cast<unsigned>(std::ceil(share * static_cast<float>(_count)));
    const float left = seen.x - _look.radius;
    const float right = seen.x + seen.width + _look.radius;
    const float top = seen.y - _look.radius;
    const float bottom = seen.y + seen.height + _look.radius;
    auto camera = paint.Camera(layer);
    for (unsigned i = 0U; i < showing; ++i) {
        const unsigned own = _seed + i * 2654435761U;
        const double cycles = _time / static_cast<double>(_look.seconds) + static_cast<double>(Share(own));
        const double whole = std::floor(cycles);
        const auto age = static_cast<float>(cycles - whole);
        // A new place each cycle, from the cycle's number: a ring needs no memory.
        const unsigned cycle = own ^ (static_cast<unsigned>(static_cast<long long>(whole)) * 0x9E3779B1U);
        const Float2 at{Share(cycle) * AtmosphereTile.x, Share(cycle + 1U) * AtmosphereTile.y};
        const float radius = _look.radius * age;
        const float width = std::min(_look.width, radius);
        if (width <= 0.0f) continue;
        const Color4 color = ScaleAlpha(_look.color, 1.0f - age);
        for (float y = at.y + AtmosphereTile.y * std::ceil((top - at.y) / AtmosphereTile.y); y <= bottom;
             y += AtmosphereTile.y) {
            for (float x = at.x + AtmosphereTile.x * std::ceil((left - at.x) / AtmosphereTile.x); x <= right;
                 x += AtmosphereTile.x) {
                paint.Ellipse({x, y}, {radius, radius * _look.squash}, Outline(width, color, StrokeAlign::Inside));
            }
        }
    }
}
