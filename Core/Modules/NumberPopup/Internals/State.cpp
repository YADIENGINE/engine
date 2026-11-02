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
} // namespace

NumberPopup::State::State(const NumberPopupSettings& settings)
    : popups({.capacity = settings.capacity, .overflow = settings.overflow, .strict = settings.strict})
    , particles({.capacity = settings.particleCapacity, .seed = settings.seed})
    , rng(settings.seed ? *settings.seed : std::random_device{}())
    , strict(settings.strict) {
    entries.reserve(settings.capacity);
}

bool NumberPopup::State::CanMutate() const {
    if (!drawing) return true;
    if (strict) throw std::logic_error("Cannot mutate or reenter NumberPopup during Draw");
    Reject("NumberPopup mutated or reentered during Draw");
    return false;
}

void NumberPopup::State::Reject(std::string_view message) const {
    if (strict) throw std::invalid_argument(std::string(message));
    if (rejected == 0) firstRejection = message;
    ++rejected;
}

void NumberPopup::State::Prune() {
    std::erase_if(entries, [](const auto& entry) { return entry.use_count() == 1; });
}

std::shared_ptr<NumberPopup::State::Entry> NumberPopup::State::Find(PopupHandle handle) const {
    const auto found = std::find_if(entries.begin(), entries.end(),
                                    [&](const auto& entry) { return entry->popup == handle && entry.use_count() > 1; });
    return found == entries.end() ? nullptr : *found;
}

bool NumberPopup::State::ValidIntensity(float intensity) {
    return std::isfinite(intensity) && intensity >= 0.0f && intensity <= 4.0f;
}

bool NumberPopup::State::ValidStyle(const NumberPopupStyle& style) {
    const auto& juice = style.juice;
    const auto& merge = style.merge;
    return Popup::IsValid(style.popup) && ValidFormat(style.format) && std::isfinite(style.text.size) &&
           style.text.size > 0.0f && Nonnegative(juice.scatterVelocity.x) && Nonnegative(juice.scatterVelocity.y) &&
           Nonnegative(juice.rotationSpread) && Nonnegative(juice.flashSeconds) && juice.particleCount >= 0 &&
           ParticleSystem2D::IsValid(juice.particles, juice.particleLook) && std::isfinite(juice.pulse.seconds) &&
           juice.pulse.seconds > 0.0f && Finite(juice.pulse.scale) && std::isfinite(juice.pulse.rotation) &&
           Nonnegative(merge.windowSeconds) && Nonnegative(merge.extendSeconds) &&
           std::isfinite(merge.maxLifetimeSeconds) && merge.maxLifetimeSeconds > 0.0f;
}

PopupHandle NumberPopup::State::Launch(Float2 position, const std::shared_ptr<Entry>& entry,
                                       const NumberPopupEmit& options) {
    const NumberPopupStyle& style = entry->style;
    entry->changedAt = clock + style.popup.delaySeconds;
    entry->intensity = options.intensity;
    entry->flashIntensity = options.intensity;
    entry->flashes = quality.flashes;
    PopupStyle animation = style.popup;
    const auto sample = [&](float spread) {
        return std::uniform_real_distribution<float>(-spread, spread)(rng) * options.intensity;
    };
    animation.motion.velocity += Float2{sample(style.juice.scatterVelocity.x), sample(style.juice.scatterVelocity.y)};
    animation.motion.rotation += sample(style.juice.rotationSpread);
    animation.motion.wobbleRadians *= options.intensity;
    animation.enter.scale = Float2{1.0f, 1.0f} + (animation.enter.scale - Float2{1.0f, 1.0f}) * options.intensity;
    animation.enter.scale.x = std::max(0.0f, animation.enter.scale.x);
    animation.enter.scale.y = std::max(0.0f, animation.enter.scale.y);
    animation.enter.rotation *= options.intensity;
    entry->popup = popups.Emit(
        position, [entry](Painter& paint, const PopupFrame& frame) { DrawEntry(paint, frame, *entry); }, animation);
    if (!entry->popup) return {};
    Prune(); // Emit may have evicted the oldest entry
    entries.push_back(entry);
    const auto frame = popups.Frame(entry->popup);
    if (frame && frame->phase != PopupPhase::Delayed) {
        Burst(*entry, frame->position, options.intensity);
        entry->pendingBurst = false;
    }
    return entry->popup;
}

void NumberPopup::State::Burst(const Entry& entry, Float2 position, float intensity) {
    const auto& juice = entry.style.juice;
    // Clamp before conversion so a valid but very large configured count cannot overflow an int.
    const double scaled = static_cast<double>(juice.particleCount) * intensity;
    const int count = static_cast<int>(std::min(scaled, static_cast<double>(std::numeric_limits<int>::max() / 100)));
    if (count == 0) return;
    ParticleEmission emission = juice.particles;
    emission.speed = {emission.speed.min * intensity, emission.speed.max * intensity};
    ParticleAppearance look = juice.particleLook;
    if (juice.particlesUseTextColor) look.color = entry.style.text.color;
    particles.Emit(position, count, emission, look);
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
    if (!entry.value) return false;
    const auto frame = popups.Frame(entry.popup);
    if (!frame || frame->phase == PopupPhase::Exit) return false;
    NumberPopupValue sum = Sum(*entry.value, delta);
    std::string text = FormatValue(sum, entry.style.format);
    entry.value = sum;
    entry.text = std::move(text);
    entry.layout.reset();
    entry.changedAt = std::max(entry.changedAt, clock);
    popups.Extend(entry.popup, entry.style.merge.extendSeconds, entry.style.merge.maxLifetimeSeconds);
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
    DrawPopupText(paint, font, *entry.layout, text);
}
