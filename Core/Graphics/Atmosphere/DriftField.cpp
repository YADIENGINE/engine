#include "Graphics/Atmosphere/DriftField.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>
#include "Graphics/Atmosphere/Internals/AtmosphereView.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Scalar.h"
#include "Utilities/Random/Rng.h"

namespace {
constexpr float DensityArea = 100.0f * 100.0f;
constexpr std::size_t MaxPieces = 40000;
constexpr float ReducedShare = 0.5f;
constexpr float TwinkleTurns = 3.0f; // Whole turns per swing, so that the phase can wrap

bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }

bool ValidRange(const FloatRange& range) {
    return std::isfinite(range.min) && std::isfinite(range.max) && range.min <= range.max;
}

float Sample(Rng& rng, const FloatRange& range) { return rng.Float(range.min, range.max); }

float Wrap(float value, float length) { return value - length * std::floor(value / length); }
} // namespace

bool IsValid(const DriftLook& look) {
    return std::isfinite(look.density) && look.density >= 0.0f && Finite(look.fall) && std::isfinite(look.windShare) &&
           ValidRange(look.pace) && ValidRange(look.size) && look.size.min >= 0.0f && ValidRange(look.spin) &&
           std::isfinite(look.sway) && look.sway >= 0.0f && std::isfinite(look.swayRate) &&
           std::isfinite(look.twinkle) && look.twinkle >= 0.0f && look.twinkle <= 1.0f &&
           std::isfinite(look.streakSeconds) && look.streakSeconds >= 0.0f && std::isfinite(look.parallax) &&
           look.parallax >= 0.0f;
}

DriftField::DriftField(const DriftLook& look, std::uint64_t seed)
    : _look(look) {
    if (!IsValid(look)) throw std::invalid_argument("Invalid drift look");
    const float wanted = look.density * AtmosphereTile.x * AtmosphereTile.y / DensityArea;
    _pieces.resize(std::min(static_cast<std::size_t>(std::ceil(wanted)), MaxPieces));
    Rng rng(seed);
    for (Piece& piece : _pieces) {
        piece.at = {rng.Float(0.0f, AtmosphereTile.x), rng.Float(0.0f, AtmosphereTile.y)};
        piece.pace = Sample(rng, look.pace);
        piece.velocity = look.fall * piece.pace;
        piece.size = Sample(rng, look.size);
        piece.phase = rng.Float(0.0f, 2.0f * Pi);
        piece.swayPace = rng.Float(0.6f, 1.4f);
        piece.rotation = rng.Float(0.0f, 2.0f * Pi);
        piece.spin = Sample(rng, look.spin);
    }
}

void DriftField::Update(float seconds, Float2 wind) {
    if (!(seconds > 0.0f) || !std::isfinite(seconds)) return;
    if (!Finite(wind)) wind = {};
    const Float2 movement = _look.fall + wind * _look.windShare;
    for (Piece& piece : _pieces) {
        piece.velocity = movement * piece.pace;
        piece.at = {Wrap(piece.at.x + piece.velocity.x * seconds, AtmosphereTile.x),
                    Wrap(piece.at.y + piece.velocity.y * seconds, AtmosphereTile.y)};
        piece.phase = Wrap(piece.phase + _look.swayRate * piece.swayPace * seconds, 2.0f * Pi);
        piece.rotation = Wrap(piece.rotation + piece.spin * seconds, 2.0f * Pi);
    }
}

void DriftField::Draw(Painter& paint, float amount, const CameraView& view) const {
    if (paint.CurrentTransform() != Affine2::Identity())
        return paint.Reject("DriftField::Draw must be called outside paint.Camera and paint.Transform");
    if (!(amount > 0.0f) || _pieces.empty()) return;
    const CameraView layer = AtmosphereLayerView(paint, view, _look.parallax);
    const FloatRect seen = AtmosphereSeenArea(layer);
    const float share = std::min(amount, 1.0f) * (paint.Quality().backgroundEffects ? 1.0f : ReducedShare) *
                        AtmosphereThinning(seen);
    const float showing = share * static_cast<float>(_pieces.size());
    const auto count = std::min(static_cast<std::size_t>(std::ceil(showing)), _pieces.size());
    float reach = _look.sway + _look.size.max * 2.0f;
    if (_look.streakSeconds > 0.0f) {
        const Float2 fastest = _pieces.front().velocity * (_look.pace.max / std::max(_pieces.front().pace, 1e-3f));
        reach += std::hypot(fastest.x, fastest.y) * _look.streakSeconds;
    }
    const float left = seen.x - reach;
    const float right = seen.x + seen.width + reach;
    const float top = seen.y - reach;
    const float bottom = seen.y + seen.height + reach;

    auto camera = paint.Camera(layer);
    std::optional<PaintScope> blend;
    if (_look.blend != Blend::Normal) blend.emplace(paint.Blending(_look.blend));
    for (std::size_t i = 0; i < count; ++i) {
        const Piece& piece = _pieces[i];
        const float swing = _look.sway > 0.0f ? std::sin(piece.phase) * _look.sway : 0.0f;
        const Float2 at{piece.at.x + swing, piece.at.y};
        float alpha = 1.0f - _look.twinkle * (0.5f + 0.5f * std::sin(piece.phase * TwinkleTurns));
        // The last one comes in by degrees, so a slowly changing amount never pops.
        if (i + 1 == count) alpha *= showing - static_cast<float>(count - 1);
        const Color4 color = ScaleAlpha(_look.color, alpha);
        if (color.a == 0) continue;
        for (float y = at.y + AtmosphereTile.y * std::ceil((top - at.y) / AtmosphereTile.y); y <= bottom;
             y += AtmosphereTile.y) {
            for (float x = at.x + AtmosphereTile.x * std::ceil((left - at.x) / AtmosphereTile.x); x <= right;
                 x += AtmosphereTile.x) {
                const ParticleDrawData data{.position = {x, y},
                                            .velocity = piece.velocity,
                                            .color = color,
                                            .size = piece.size,
                                            .rotation = piece.rotation};
                if (_look.draw)
                    _look.draw(paint, data);
                else
                    DrawParticleShape(paint, _look.shape, data,
                                      {.sparkTrailSeconds = _look.streakSeconds, .sparkThickness = piece.size});
            }
        }
    }
}
