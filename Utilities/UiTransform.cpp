#include "Utilities/UiTransform.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

UiTransform::UiTransform(Rect source, Rect target)
    : _sourceCenter{source.CenterX(), source.CenterY()}
    , _targetCenter{target.CenterX(), target.CenterY()} {
    if (source.width <= 0 || source.height <= 0 || target.width <= 0 || target.height <= 0) {
        throw std::invalid_argument("UI transform rectangles must have positive dimensions");
    }
    _scale = std::min(static_cast<float>(target.width) / static_cast<float>(source.width),
                      static_cast<float>(target.height) / static_cast<float>(source.height));
    _offset = {static_cast<float>(_targetCenter.x) - static_cast<float>(_sourceCenter.x) * _scale,
               static_cast<float>(_targetCenter.y) - static_cast<float>(_sourceCenter.y) * _scale};
}

Float2 UiTransform::Map(Float2 point) const { return {point.x * _scale + _offset.x, point.y * _scale + _offset.y}; }
Float2 UiTransform::Unmap(Float2 point) const {
    return {(point.x - _offset.x) / _scale, (point.y - _offset.y) / _scale};
}
Vec2 UiTransform::Map(Vec2 point) const {
    // Keep integer layout targets centered before rounding, including half-pixel ties.
    return {static_cast<int>(std::lround(static_cast<float>(_targetCenter.x) +
                                         static_cast<float>(point.x - _sourceCenter.x) * _scale)),
            static_cast<int>(std::lround(static_cast<float>(_targetCenter.y) +
                                         static_cast<float>(point.y - _sourceCenter.y) * _scale))};
}
Vec2 UiTransform::Unmap(Vec2 point) const {
    const Float2 local = Unmap(Float2{static_cast<float>(point.x), static_cast<float>(point.y)});
    return {static_cast<int>(std::lround(local.x)), static_cast<int>(std::lround(local.y))};
}
