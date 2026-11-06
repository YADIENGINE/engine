#include "Graphics/Atmosphere/FogField.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Graphics/Atmosphere/Internals/AtmosphereView.h"
#include "Graphics/Paint/Painter.h"
#include "Utilities/Math/Noise.h"
#include "Utilities/Math/NoiseField.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr float MinCell = 4.0f;
constexpr float ThinnestEdge = 0.66f; // Below this noise there is no fog at the smallest amount
constexpr float ThickestEdge = 0.16f; // ... and at full amount
constexpr float EdgeWidth = 0.42f;    // How softly a patch ends, in noise
constexpr float FadeInAmount = 0.15f; // Below this amount the whole sheet fades too, so that it starts from nothing
constexpr float ReducedCellScale = 2.0f;
// Each octave moves at its own pace and boils in its own direction: the shape changes as it drifts.
constexpr std::array OctavePace{1.0f, 1.45f, 0.7f};
constexpr std::array OctaveBoil{Float2{0.8f, 0.6f}, Float2{-0.7f, 0.7f}, Float2{0.3f, -1.0f}};
constexpr std::array OctaveScale{1.0f, 2.1f, 4.3f};
constexpr std::array OctaveWeight{0.56f, 0.3f, 0.14f};
constexpr float OffsetWrap = 1048576.0f; // Noise has no period, but this keeps floats fine over a long session

bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }

float SmoothRange(float from, float to, float value) {
    const float t = std::clamp((value - from) / (to - from), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float BandShare(const FogBand& band, float y) {
    if (band.soft <= 0.0f) return y >= band.top && y <= band.bottom ? 1.0f : 0.0f;
    return SmoothRange(band.top - band.soft, band.top + band.soft, y) *
           (1.0f - SmoothRange(band.bottom - band.soft, band.bottom + band.soft, y));
}
} // namespace

bool IsValid(const FogLook& look) {
    const bool band = !look.band || (std::isfinite(look.band->top) && std::isfinite(look.band->bottom) &&
                                     std::isfinite(look.band->soft) && look.band->soft >= 0.0f &&
                                     look.band->top <= look.band->bottom);
    return band && std::isfinite(look.wavelength) && look.wavelength > 0.0f && Finite(look.drift) &&
           std::isfinite(look.windShare) && std::isfinite(look.boil) && std::isfinite(look.cell) &&
           look.cell >= MinCell && std::isfinite(look.parallax) && look.parallax >= 0.0f;
}

FogField::FogField(const FogLook& look, std::uint64_t seed)
    : _look(look)
    , _seed(Hash(static_cast<unsigned>(seed))) {
    if (!IsValid(look)) throw std::invalid_argument("Invalid fog look");
}

void FogField::Update(float seconds, Float2 wind) {
    if (!(seconds > 0.0f) || !std::isfinite(seconds)) return;
    if (!Finite(wind)) wind = {};
    const Float2 movement = _look.drift + wind * _look.windShare;
    for (std::size_t i = 0; i < Octaves; ++i) {
        const Float2 next = _offsets[i] + (movement * OctavePace[i] + OctaveBoil[i] * _look.boil) * seconds;
        _offsets[i] = {std::fmod(next.x, OffsetWrap), std::fmod(next.y, OffsetWrap)};
    }
}

void FogField::Draw(Painter& paint, float amount, const CameraView& view) const {
    if (paint.CurrentTransform() != Affine2::Identity())
        return paint.Reject("FogField::Draw must be called outside paint.Camera and paint.Transform");
    if (!(amount > 0.0f) || _look.color.a == 0) return;
    const float thickness = std::min(amount, 1.0f);
    const CameraView layer = AtmosphereLayerView(paint, view, _look.parallax);
    const FloatRect& box = layer.viewport;
    if (box.Empty()) return;

    const float cell = _look.cell * (paint.Quality().backgroundEffects ? 1.0f : ReducedCellScale);
    const int columns = std::max(static_cast<int>(std::ceil(box.width / cell)), 1);
    const int rows = std::max(static_cast<int>(std::ceil(box.height / cell)), 1);
    const auto stride = static_cast<std::uint32_t>(columns + 1);
    if (columns != _columns || rows != _rows) {
        _columns = columns;
        _rows = rows;
        _indices.clear();
        for (std::uint32_t row = 0; row < static_cast<std::uint32_t>(rows); ++row) {
            for (std::uint32_t column = 0; column < static_cast<std::uint32_t>(columns); ++column) {
                const std::uint32_t at = row * stride + column;
                _indices.insert(_indices.end(), {at, at + 1, at + stride, at + 1, at + stride + 1, at + stride});
            }
        }
    }
    const std::size_t count = static_cast<std::size_t>(stride) * static_cast<std::size_t>(rows + 1);
    _points.resize(count);
    _colors.resize(count);

    const float edge = Mix(ThinnestEdge, ThickestEdge, thickness);
    const float fade = std::min(thickness / FadeInAmount, 1.0f);
    const Affine2 toWorld = layer.Transform().Inverse();
    std::size_t index = 0;
    for (int row = 0; row <= rows; ++row) {
        const float y = box.y + box.height * static_cast<float>(row) / static_cast<float>(rows);
        for (int column = 0; column <= columns; ++column, ++index) {
            const float x = box.x + box.width * static_cast<float>(column) / static_cast<float>(columns);
            const Float2 world = toWorld.Apply({x, y});
            float noise = 0.0f;
            for (std::size_t i = 0; i < Octaves; ++i) {
                const float scale = OctaveScale[i] / _look.wavelength;
                const float px = (world.x - _offsets[i].x) * scale;
                const float py = (world.y - _offsets[i].y) * scale;
                const NoiseSettings octave{.seed = _seed + static_cast<unsigned>(i) * 131U};
                noise += OctaveWeight[i] * (Noise(octave, Float2{px, py}) * 0.5f + 0.5f);
            }
            float share = SmoothRange(edge, edge + EdgeWidth, noise) * fade;
            if (_look.band) share *= BandShare(*_look.band, world.y);
            _points[index] = {x, y};
            _colors[index] = ScaleAlpha(_look.color, share);
        }
    }
    auto clip = paint.Clip(box);
    paint.Mesh(_points, _colors, _indices);
}
