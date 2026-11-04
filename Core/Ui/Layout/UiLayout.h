#pragma once
#include <functional>
#include <limits>
#include <optional>
#include "Utilities/Float2.h"

inline constexpr float UiUnlimited = std::numeric_limits<float>::infinity();

// Fit: as large as the content. Fixed: 'value' units. Fill: a share of the room left in the parent, 'value' being the
// weight along the parent's direction; across it, the whole width of the parent. 'min' and 'max' bound the result.
enum class UiSizeKind { Fit, Fixed, Fill };
struct UiSize {
    UiSizeKind kind = UiSizeKind::Fit;
    float value = 0.0f;
    float min = 0.0f;
    float max = UiUnlimited;
};
[[nodiscard]] constexpr UiSize UiFit(float min = 0.0f, float max = UiUnlimited) {
    return {UiSizeKind::Fit, 0.0f, min, max};
}
[[nodiscard]] constexpr UiSize UiFixed(float units) { return {UiSizeKind::Fixed, units}; }
[[nodiscard]] constexpr UiSize UiFill(float weight = 1.0f, float min = 0.0f, float max = UiUnlimited) {
    return {UiSizeKind::Fill, weight, min, max};
}

// How children follow each other. Overlay: on top of each other, each aligned on its own.
enum class UiDirection { Column, Row, Overlay };
enum class UiAlign { Start, Center, End };
// Where an anchored child sits in its parent, out of the flow of its siblings.
enum class UiAnchor { TopLeft, Top, TopRight, Left, Center, Right, BottomLeft, Bottom, BottomRight };

struct UiInsets {
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;

    [[nodiscard]] static constexpr UiInsets All(float amount) { return {amount, amount, amount, amount}; }
    [[nodiscard]] static constexpr UiInsets Symmetric(float horizontal, float vertical) {
        return {horizontal, vertical, horizontal, vertical};
    }
    friend constexpr bool operator==(const UiInsets&, const UiInsets&) = default;
};

// The size of a leaf's content, given the width it may use (UiUnlimited: as wide as it likes).
using UiMeasure = std::function<Float2(float maxWidth)>;

// An element's size, how it arranges its children, and where it goes when anchored. Pure data.
struct UiBox {
    UiSize width{};
    UiSize height{};
    UiDirection direction = UiDirection::Column;
    float gap = 0.0f;
    UiInsets padding{};
    UiAlign alignX = UiAlign::Start;
    UiAlign alignY = UiAlign::Start;
    std::optional<UiAnchor> anchor{};
    Float2 offset{}; // Added to an anchored element's place; a look's movement belongs in UiVisual
};
