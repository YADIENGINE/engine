#include "Graphics/Paint/Internals/ShapeArea.h"
#include <clipper2/clipper.h>
#include <algorithm>
#include <cmath>

namespace {
Clipper2Lib::JoinType ToJoin(StrokeJoin join) {
    switch (join) {
    case StrokeJoin::Miter: return Clipper2Lib::JoinType::Miter;
    case StrokeJoin::Bevel: return Clipper2Lib::JoinType::Bevel;
    case StrokeJoin::Round: return Clipper2Lib::JoinType::Round;
    }
    return Clipper2Lib::JoinType::Miter;
}

void AddRing(const Clipper2Lib::Path64& ring, double unit, Regions& out) {
    const auto first = static_cast<std::uint32_t>(out.points.size());
    for (const Clipper2Lib::Point64& point : ring)
        out.points.push_back({static_cast<float>(static_cast<double>(point.x) * unit),
                              static_cast<float>(static_cast<double>(point.y) * unit)});
    out.rings.push_back({first, static_cast<std::uint32_t>(ring.size()), true});
}

// Each outline with its holes becomes a region; islands inside a hole are outlines of their own.
void AddRegions(const Clipper2Lib::PolyPath64& outline, double unit, Regions& out) {
    AddRing(outline.Polygon(), unit, out);
    for (const auto& hole : outline)
        AddRing(hole->Polygon(), unit, out);
    out.ends.push_back(static_cast<std::uint32_t>(out.rings.size()));
    for (const auto& hole : outline)
        for (const auto& island : *hole)
            AddRegions(*island, unit, out);
}

void GetRegions(const Clipper2Lib::PolyTree64& tree, double unit, Regions& out) {
    out.Clear();
    for (const auto& outline : tree)
        AddRegions(*outline, unit, out);
}
} // namespace

struct ShapeArea::State {
    Clipper2Lib::PolyTree64 tree; // Outlines with their holes
    Clipper2Lib::Paths64 area;    // The same rings, flat, for offsetting
    double unit = 1.0;
};

ShapeArea::ShapeArea()
    : _state(std::make_unique<State>()) {}
ShapeArea::~ShapeArea() = default;
ShapeArea::ShapeArea(ShapeArea&&) noexcept = default;
ShapeArea& ShapeArea::operator=(ShapeArea&&) noexcept = default;

void ShapeArea::Set(std::span<const Float2> points, std::span<const Contour> contours, FillRule rule, float unit) {
    State& state = *_state;
    state.unit = std::max(static_cast<double>(unit), 1e-9);
    const double scale = 1.0 / state.unit;
    Clipper2Lib::Paths64 subject;
    subject.reserve(contours.size());
    for (const Contour& contour : contours) {
        if (contour.count < 3) continue;
        Clipper2Lib::Path64& ring = subject.emplace_back();
        ring.reserve(contour.count);
        for (const Float2 point : points.subspan(contour.first, contour.count)) {
            if (!std::isfinite(point.x) || !std::isfinite(point.y)) continue;
            ring.emplace_back(std::llround(static_cast<double>(point.x) * scale),
                              std::llround(static_cast<double>(point.y) * scale));
        }
    }
    const auto clipperRule =
        rule == FillRule::EvenOdd ? Clipper2Lib::FillRule::EvenOdd : Clipper2Lib::FillRule::NonZero;
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(subject);
    clipper.Execute(Clipper2Lib::ClipType::Union, clipperRule, state.tree);
    state.area = Clipper2Lib::PolyTreeToPaths64(state.tree);
}

bool ShapeArea::Empty() const { return _state->area.empty(); }

void ShapeArea::Get(Regions& out) const { GetRegions(_state->tree, _state->unit, out); }

void ShapeArea::Band(float distance, StrokeJoin join, float miterLimit, float roundTolerance, Regions& out) const {
    out.Clear();
    const State& state = *_state;
    if (state.area.empty() || !std::isfinite(distance) || distance == 0.0f) return;
    const double delta = static_cast<double>(distance) / state.unit;
    const double arcTolerance = std::max(static_cast<double>(roundTolerance) / state.unit, 1.0);
    Clipper2Lib::ClipperOffset offset(std::max(static_cast<double>(miterLimit), 1.0), arcTolerance);
    offset.AddPaths(state.area, ToJoin(join), Clipper2Lib::EndType::Polygon);
    Clipper2Lib::Paths64 moved;
    offset.Execute(delta, moved);
    Clipper2Lib::Clipper64 clipper;
    clipper.AddSubject(delta > 0.0 ? moved : state.area);
    clipper.AddClip(delta > 0.0 ? state.area : moved);
    Clipper2Lib::PolyTree64 tree;
    clipper.Execute(Clipper2Lib::ClipType::Difference, Clipper2Lib::FillRule::NonZero, tree);
    GetRegions(tree, state.unit, out);
}
