#include "Fx/World/Marks.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include "Fx/Internals/FxChecks.h"
#include "Graphics/Paint/Painter.h"

bool IsValid(const MarkStyle& style) {
    return FxPositive(style.radius) && FxPositive(style.seconds) && FxNotNegative(style.fadeSeconds) &&
           FxNotNegative(style.growSeconds) && FxPositive(style.flatten);
}

void Marks::Leave(Float2 at, Color4 color, const MarkStyle& style) {
    if (!IsValid(style)) throw std::invalid_argument("Invalid mark style");
    if (_lying.size() >= MaxLying) _lying.erase(_lying.begin());
    _lying.push_back({at, color, style});
}

void Marks::Update(float deltaSeconds) {
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (Lying& mark : _lying)
        mark.age += deltaSeconds;
    std::erase_if(_lying, [](const Lying& mark) { return mark.age >= mark.style.seconds; });
}

void Marks::Clear() { _lying.clear(); }

void Marks::Draw(Painter& paint) const {
    for (const Lying& mark : _lying) {
        const MarkStyle& style = mark.style;
        const float grown = style.growSeconds > 0.0f ? std::min(1.0f, mark.age / style.growSeconds) : 1.0f;
        const float left = style.seconds - mark.age;
        const float shown = style.fadeSeconds > 0.0f ? std::clamp(left / style.fadeSeconds, 0.0f, 1.0f) : 1.0f;
        const float radius = style.radius * grown;
        paint.Ellipse(mark.at, {radius, radius * style.flatten}, Fill(ScaleAlpha(mark.color, shown)));
    }
}
