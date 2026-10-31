#pragma once
#include <algorithm>
#include "Utilities/Float2.h"
#include "Utilities/Rect.h"

// Axis-aligned rectangle in continuous coordinates; x/y is the top-left corner (y grows downwards). Header-only.
struct FloatRect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;

    constexpr FloatRect() = default;
    constexpr FloatRect(float left, float top, float w, float h)
        : x(left)
        , y(top)
        , width(w)
        , height(h) {}

    [[nodiscard]] static constexpr FloatRect FromCenter(Float2 center, Float2 size) {
        return {center.x - size.x * 0.5f, center.y - size.y * 0.5f, size.x, size.y};
    }
    [[nodiscard]] static constexpr FloatRect FromCorners(Float2 a, Float2 b) {
        const float left = std::min(a.x, b.x);
        const float top = std::min(a.y, b.y);
        return {left, top, std::max(a.x, b.x) - left, std::max(a.y, b.y) - top};
    }
    [[nodiscard]] static constexpr FloatRect From(const Rect& rect) {
        return {static_cast<float>(rect.x), static_cast<float>(rect.y), static_cast<float>(rect.width),
                static_cast<float>(rect.height)};
    }

    [[nodiscard]] constexpr float Right() const { return x + width; }
    [[nodiscard]] constexpr float Bottom() const { return y + height; }
    [[nodiscard]] constexpr Float2 Position() const { return {x, y}; }
    [[nodiscard]] constexpr Float2 Size() const { return {width, height}; }
    [[nodiscard]] constexpr Float2 Center() const { return {x + width * 0.5f, y + height * 0.5f}; }
    [[nodiscard]] constexpr bool Empty() const { return !(width > 0.0f) || !(height > 0.0f); }
    // Half-open: the left/top edges are inside, the right/bottom edges are not.
    [[nodiscard]] constexpr bool Contains(Float2 point) const {
        return point.x >= x && point.x < Right() && point.y >= y && point.y < Bottom();
    }
    // Each edge moves 'amount' inwards (negative: outwards); the size never goes below zero.
    [[nodiscard]] constexpr FloatRect Inset(float amount) const { return Inset(amount, amount); }
    [[nodiscard]] constexpr FloatRect Inset(float horizontal, float vertical) const {
        const float dx = std::min(horizontal, width * 0.5f);
        const float dy = std::min(vertical, height * 0.5f);
        return {x + dx, y + dy, width - 2.0f * dx, height - 2.0f * dy};
    }
    [[nodiscard]] constexpr FloatRect Translated(Float2 offset) const {
        return {x + offset.x, y + offset.y, width, height};
    }
    // Overlap of both rectangles; empty (zero size, placed at the clamped corner) when they do not overlap.
    [[nodiscard]] constexpr FloatRect Intersect(const FloatRect& other) const {
        const float left = std::max(x, other.x);
        const float top = std::max(y, other.y);
        const float right = std::max(left, std::min(Right(), other.Right()));
        const float bottom = std::max(top, std::min(Bottom(), other.Bottom()));
        return {left, top, right - left, bottom - top};
    }
    // Smallest rectangle containing both; an empty rectangle does not extend the other.
    [[nodiscard]] constexpr FloatRect Union(const FloatRect& other) const {
        if (Empty()) return other;
        if (other.Empty()) return *this;
        const float left = std::min(x, other.x);
        const float top = std::min(y, other.y);
        return {left, top, std::max(Right(), other.Right()) - left, std::max(Bottom(), other.Bottom()) - top};
    }

    friend constexpr bool operator==(const FloatRect&, const FloatRect&) = default;
};

constexpr Rect::operator FloatRect() const { return FloatRect::From(*this); }
