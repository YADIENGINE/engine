#include "Ui/Kernel/Internals/UiFocusSearch.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
// Being off to the side counts this many times more than being far ahead, so a list stays a list.
constexpr float SideWeight = 3.0f;

struct Span1D {
    float from = 0.0f;
    float to = 0.0f;
    [[nodiscard]] float Center() const { return (from + to) * 0.5f; }
    // Space between the two spans; zero when they overlap.
    [[nodiscard]] float Gap(const Span1D& other) const { return std::max({0.0f, other.from - to, from - other.to}); }
};
[[nodiscard]] Span1D Along(const FloatRect& rect, UiMove move) {
    if (move == UiMove::Left || move == UiMove::Right) return {rect.x, rect.Right()};
    return {rect.y, rect.Bottom()};
}
[[nodiscard]] Span1D Across(const FloatRect& rect, UiMove move) {
    if (move == UiMove::Left || move == UiMove::Right) return {rect.y, rect.Bottom()};
    return {rect.x, rect.Right()};
}
[[nodiscard]] bool Forward(UiMove move) { return move == UiMove::Down || move == UiMove::Right; }
} // namespace

std::optional<std::size_t> FindUiFocusTarget(std::span<const FloatRect> candidates, const FloatRect& from, UiMove move,
                                             bool wrap) {
    const float sign = Forward(move) ? 1.0f : -1.0f;
    const Span1D fromAlong = Along(from, move);
    const Span1D fromAcross = Across(from, move);
    std::optional<std::size_t> ahead;
    std::optional<std::size_t> behind;
    float bestAhead = std::numeric_limits<float>::infinity();
    float bestBehind = std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const Span1D along = Along(candidates[index], move);
        const Span1D across = Across(candidates[index], move);
        const float travel = (along.Center() - fromAlong.Center()) * sign;
        const float side = across.Gap(fromAcross);
        const float centering = std::abs(across.Center() - fromAcross.Center()) * 0.01f;
        if (travel > 0.5f) {
            const float score = travel + side * SideWeight + centering;
            if (score < bestAhead) {
                bestAhead = score;
                ahead = index;
            }
        } else if (travel < -0.5f) {
            // The farthest one behind is where the row or column starts again.
            const float score = travel + side * SideWeight + centering;
            if (score < bestBehind) {
                bestBehind = score;
                behind = index;
            }
        }
    }
    if (ahead) return ahead;
    return wrap ? behind : std::nullopt;
}

std::optional<std::size_t> FindNearestUiFocus(std::span<const FloatRect> candidates, const FloatRect& from) {
    std::optional<std::size_t> nearest;
    float best = std::numeric_limits<float>::infinity();
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const Float2 delta = candidates[index].Center() - from.Center();
        const float distance = delta.x * delta.x + delta.y * delta.y;
        if (distance < best) {
            best = distance;
            nearest = index;
        }
    }
    return nearest;
}
