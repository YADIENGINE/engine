#include "Modules/NumberPopup/Internals/State.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/FontFace.h"
#include "Utilities/MathUtils.h"

namespace {
bool Nonnegative(float value) { return std::isfinite(value) && value >= 0.0f; }
bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }
bool Range(FloatRange range) { return Nonnegative(range.min) && std::isfinite(range.max) && range.max >= range.min; }
} // namespace

NumberPopup::State::State(const NumberPopupSettings& settings)
    : popups({.capacity = settings.capacity, .overflow = settings.overflow})
    , particles({.capacity = settings.particleCapacity, .seed = settings.seed})
    , rng(settings.seed ? *settings.seed : std::random_device{}()) {
    entries.reserve(settings.capacity);
}

void NumberPopup::State::RequireMutable() const {
    if (drawing) throw std::logic_error("Cannot mutate or reenter NumberPopup during Draw");
}

void NumberPopup::State::Prune() {
    std::erase_if(entries, [&](const auto& entry) { return !popups.IsAlive(entry->popup); });
}

std::shared_ptr<NumberPopup::State::Entry> NumberPopup::State::Find(PopupHandle handle) const {
    if (!popups.IsAlive(handle)) return {};
    const auto found =
        std::find_if(entries.begin(), entries.end(), [&](const auto& entry) { return entry->popup == handle; });
    return found == entries.end() ? nullptr : *found;
}

void NumberPopup::State::ValidateIntensity(float intensity) {
    if (!std::isfinite(intensity) || intensity < 0.0f || intensity > 4.0f)
        throw std::invalid_argument("Number popup intensity must be between 0 and 4");
}

void NumberPopup::State::ValidateStyle(const NumberPopupStyle& style) {
    (void)Popup::Duration(style.popup);
    ValidateFormat(style.format);
    const auto& text = style.text;
    const auto& juice = style.juice;
    if (!std::isfinite(text.size) || text.size <= 0.0f || !Nonnegative(text.maxWidth) ||
        !std::isfinite(text.letterSpacing) || !Nonnegative(text.lineSpacing) || !Nonnegative(text.lineHeight) ||
        !Nonnegative(text.outline.width) || !Finite(text.shadow.offset) || !Nonnegative(text.glow.radius) ||
        !Nonnegative(text.glow.strength) || !Nonnegative(text.emission) || !Finite(juice.scatterVelocity) ||
        juice.scatterVelocity.x < 0.0f || juice.scatterVelocity.y < 0.0f ||
        !Nonnegative(juice.scatterVelocity.x * 8.0f) || !Nonnegative(juice.scatterVelocity.y * 8.0f) ||
        !Nonnegative(juice.rotationSpread * 8.0f) || !Nonnegative(juice.flashSeconds) || juice.particleCount < 0 ||
        !Range(juice.particleSpeed) || !Nonnegative(juice.particleSpeed.max * 4.0f) || !Range(juice.particleLifetime) ||
        juice.particleLifetime.min <= 0.0f || !Range(juice.particleSize) || !Finite(juice.particleAcceleration) ||
        !std::isfinite(juice.pulse.seconds) || juice.pulse.seconds <= 0.0f || !Finite(juice.pulse.scale) ||
        juice.pulse.scale.x < 0.0f || juice.pulse.scale.y < 0.0f || !Nonnegative(juice.pulse.scale.x * 4.0f) ||
        !Nonnegative(juice.pulse.scale.y * 4.0f) || !std::isfinite(juice.pulse.rotation * 4.0f) ||
        !Nonnegative(style.merge.windowSeconds) || !std::isfinite(style.merge.maxLifetimeSeconds) ||
        style.merge.maxLifetimeSeconds <= 0.0f)
        throw std::invalid_argument("Invalid number popup style");
    switch (juice.particleShape) {
    case ParticleShape::Circle:
    case ParticleShape::Square:
    case ParticleShape::Pixel:
    case ParticleShape::Spark:
    case ParticleShape::Star  : break;
    default                   : throw std::invalid_argument("Invalid number popup particle shape");
    }
}

void NumberPopup::State::Burst(const Entry& entry, Float2 position, float intensity) {
    const auto& juice = entry.style.juice;
    // Clamp before conversion so a valid but very large configured count cannot overflow an int.
    const double scaled = static_cast<double>(juice.particleCount) * intensity;
    const int count = static_cast<int>(std::min(scaled, static_cast<double>(std::numeric_limits<int>::max() / 100)));
    if (count == 0) return;
    particles.Emit(position, count,
                   {.speed = {juice.particleSpeed.min * intensity, juice.particleSpeed.max * intensity},
                    .lifetime = juice.particleLifetime,
                    .size = juice.particleSize,
                    .rotation = {-Pi, Pi},
                    .spin = {-4.0f, 4.0f},
                    .acceleration = juice.particleAcceleration,
                    .drag = 2.0f},
                   {.color = entry.style.text.color,
                    .shape = juice.particleShape,
                    .blend = ParticleBlend::Additive,
                    .endSizeScale = 0.2f,
                    .fadeFraction = 0.8f,
                    .sparkTrailSeconds = 0.035f,
                    .sparkThickness = 0.5f});
}

bool NumberPopup::State::Emphasize(Entry& entry, float intensity) {
    const auto frame = popups.Frame(entry.popup);
    if (!frame || frame->phase == PopupPhase::Exit) return false;
    PopupPulse pulse = entry.style.juice.pulse;
    pulse.scale *= intensity;
    pulse.rotation *= intensity;
    popups.Pulse(entry.popup, pulse);
    entry.flashAt = frame->ageSeconds;
    entry.flashIntensity = intensity;
    if (frame->phase != PopupPhase::Delayed) Burst(entry, frame->position, intensity * 0.5f);
    return true;
}

bool NumberPopup::State::Accumulate(Entry& entry, NumberPopupValue delta, float intensity) {
    const auto frame = popups.Frame(entry.popup);
    if (!frame || frame->phase == PopupPhase::Exit) return false;
    NumberPopupValue sum = Sum(entry.value, delta);
    std::string text = FormatValue(sum, entry.style.format);
    entry.value = sum;
    entry.text = std::move(text);
    entry.layout.reset();
    entry.changedAt = clock;
    popups.Extend(entry.popup, entry.style.merge.windowSeconds, entry.style.merge.maxLifetimeSeconds);
    return Emphasize(entry, intensity);
}

void NumberPopup::State::DrawEntry(Painter& paint, const PopupFrame& frame, const Entry& entry) {
    if (entry.text.empty()) return;
    const FontFace& font = paint.Font();
    if (!entry.layout || entry.layout->FontId() != font.Id()) entry.layout = font.Layout(entry.text, entry.style.text);
    TextStyle text = entry.style.text;
    if (entry.flashes && entry.style.juice.flashSeconds > 0.0f) {
        const float flash =
            1.0f - EaseOutCubic(Since(frame.ageSeconds, entry.flashAt) / entry.style.juice.flashSeconds);
        text.color = Mix(text.color, entry.style.juice.flashColor, flash * std::min(1.0f, entry.flashIntensity));
    }
    const Float2 size = entry.layout->Extent();
    const float side = text.align == TextAlign::Left ? 0.0f : text.align == TextAlign::Right ? 1.0f : 0.5f;
    paint.Text(font, *entry.layout, {size.x * (side - 0.5f), -size.y * 0.5f}, text);
}
