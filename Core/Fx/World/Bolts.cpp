#include "Fx/World/Bolts.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Paint/Painter.h"

namespace {
constexpr int Subdivisions = 5; // Doublings of the main line's segments
constexpr int BranchSubdivisions = 3;
constexpr float BranchWidthShare = 0.45f;
constexpr float HaloWidthScale = 3.5f; // The wide faint line around each line
constexpr float HaloAlpha = 0.22f;
constexpr float AboveArea = 40.0f; // How far above the area it starts, so its start is never seen

// A jagged line: each pass moves the middle of every segment sideways, by half as much as the last.
std::vector<Float2> JaggedLine(Rng& rng, Float2 from, Float2 to, float wander, int subdivisions) {
    std::vector<Float2> points{from, to};
    for (int pass = 0; pass < subdivisions; ++pass) {
        std::vector<Float2> finer;
        finer.reserve(points.size() * 2U);
        for (std::size_t i = 0; i + 1 < points.size(); ++i) {
            const Float2 along = points[i + 1] - points[i];
            const Float2 sideways{-along.y, along.x};
            const float length = std::max(std::hypot(along.x, along.y), 1e-3f);
            finer.push_back(points[i]);
            finer.push_back((points[i] + points[i + 1]) * 0.5f + sideways * (rng.Float(-wander, wander) / length));
        }
        finer.push_back(points.back());
        points = std::move(finer);
        wander *= 0.5f;
    }
    return points;
}

// Bright at once, dim, bright again, gone: how a bolt flickers.
float Brightness(float progress) {
    if (progress < 0.3f) return 1.0f;
    if (progress < 0.45f) return 0.25f;
    if (progress < 0.7f) return 0.9f;
    return (1.0f - progress) / 0.3f * 0.6f;
}
} // namespace

bool IsValid(const BoltStyle& style) {
    return FxPositive(style.seconds) && FxPositive(style.width) && FxNotNegative(style.jag) && style.branches >= 0 &&
           FxNotNegative(style.emission);
}

void Bolts::SetQuality(const FxQuality& quality) {
    _quality = quality;
    if (!quality.flashes) _playing.clear();
}

void Bolts::Strike(FlashLimit& limit, Float2 at, const FloatRect& area, Color4 color, const BoltStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid bolt style");
    if (!_quality.flashes || !limit.Start(FlashKind::Bolt)) return;
    const float height = std::max(at.y - area.y + AboveArea, 1.0f);
    const Float2 from{at.x + _rng.Float(-0.35f, 0.35f) * height, at.y - height};
    const float wander = style.jag * height;
    Playing playing{.lines = {}, .color = color, .style = style};
    playing.lines.push_back({JaggedLine(_rng, from, at, wander, Subdivisions), style.width});
    for (int i = 0; i < style.branches; ++i) {
        const std::vector<Float2>& main = playing.lines.front().points;
        const Float2 start = main[_rng.Index(main.size() - 1U)];
        const Float2 end = start + Float2{_rng.Float(-0.4f, 0.4f) * height, _rng.Float(0.12f, 0.3f) * height};
        playing.lines.push_back(
            {JaggedLine(_rng, start, end, wander * 0.3f, BranchSubdivisions), style.width * BranchWidthShare});
    }
    if (_playing.size() == MaxPlaying) _playing.erase(_playing.begin());
    _playing.push_back(std::move(playing));
}

void Bolts::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Playing& playing : _playing)
        playing.elapsed += deltaSeconds;
    std::erase_if(_playing, [](const Playing& playing) { return playing.elapsed >= playing.style.seconds; });
}

void Bolts::Clear() { _playing.clear(); }

void Bolts::Draw(Painter& paint) const {
    for (const Playing& playing : _playing) {
        const float bright = Brightness(playing.elapsed / playing.style.seconds);
        for (const Line& line : playing.lines) {
            paint.Polyline(line.points, {.width = line.width * HaloWidthScale,
                                         .color = ScaleAlpha(playing.color, bright * HaloAlpha),
                                         .join = StrokeJoin::Round,
                                         .emission = playing.style.emission * bright * HaloAlpha});
            paint.Polyline(line.points, {.width = line.width,
                                         .color = ScaleAlpha(playing.color, bright),
                                         .join = StrokeJoin::Round,
                                         .emission = playing.style.emission * bright});
        }
    }
}
