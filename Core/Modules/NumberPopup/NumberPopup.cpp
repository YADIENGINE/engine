#include "Modules/NumberPopup/NumberPopup.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include "Modules/NumberPopup/Internals/State.h"
#include "Utilities/MathUtils.h"

NumberPopup::NumberPopup(const NumberPopupSettings& settings)
    : _state(std::make_unique<State>(settings)) {}

NumberPopup::~NumberPopup() = default;

NumberPopupStyle NumberPopup::Preset(NumberPopupPreset preset) {
    NumberPopupStyle style;
    switch (preset) {
    case NumberPopupPreset::Float:
        style.popup = Popup::Preset(PopupPreset::Float);
        style.juice = {.scatterVelocity = {}, .rotationSpread = 0.0f, .flashSeconds = 0.0f};
        break;
    case NumberPopupPreset::Pop: break;
    case NumberPopupPreset::Burst:
        style.popup = Popup::Preset(PopupPreset::Bounce);
        style.popup.enter.rotation = -0.08f;
        style.popup.motion.wobbleRadians = 0.1f;
        style.popup.motion.wobbleFrequency = 5.0f;
        style.popup.motion.wobbleDamping = 6.0f;
        style.juice.particleCount = 10;
        style.juice.scatterVelocity = {55.0f, 20.0f};
        style.text.size = 32.0f;
        break;
    case NumberPopupPreset::Heavy:
        style.popup.enter.scale = {1.8f, 0.5f};
        style.popup.enter.seconds = 0.16f;
        style.popup.holdSeconds = 0.55f;
        style.popup.motion.velocity = {0.0f, -65.0f};
        style.popup.motion.wobbleRadians = 0.13f;
        style.popup.motion.wobbleFrequency = 6.0f;
        style.popup.motion.wobbleDamping = 7.0f;
        style.juice.particleCount = 18;
        style.juice.particleSpeed = {100.0f, 220.0f};
        style.juice.flashSeconds = 0.1f;
        style.juice.pulse.scale = {0.3f, 0.3f};
        style.text.size = 42.0f;
        style.text.outline.width = 2.0f;
        break;
    default: throw std::invalid_argument("Invalid number popup preset");
    }
    return style;
}

std::string NumberPopup::Format(NumberPopupValue value, const NumberPopupFormat& format) {
    return State::FormatValue(value, format);
}

NumberPopupHandle NumberPopup::Emit(Float2 position, NumberPopupValue value, const NumberPopupStyle& style,
                                    float intensity, NumberPopupMergeKey mergeKey) {
    _state->RequireMutable();
    State::ValidateIntensity(intensity);
    State::ValidateStyle(style);
    State::ValidateValue(value);
    if (!std::isfinite(position.x) || !std::isfinite(position.y))
        throw std::invalid_argument("Number popup position must be finite");
    _state->Prune();
    if (mergeKey.value != 0 && style.merge.windowSeconds > 0.0f) {
        const auto sign = [](const NumberPopupValue& number) {
            return std::visit([](auto v) { return (v > 0) - (v < 0); }, number);
        };
        for (auto at = _state->entries.rbegin(); at != _state->entries.rend(); ++at) {
            auto& entry = **at;
            if (entry.mergeKey != mergeKey || entry.style.merge.windowSeconds == 0.0f ||
                _state->clock - entry.changedAt > entry.style.merge.windowSeconds ||
                sign(entry.value) * sign(value) < 0)
                continue;
            if (_state->Accumulate(entry, value, intensity)) {
                NumberPopupHandle handle;
                handle._popup = entry.popup;
                return handle;
            }
        }
    }
    auto entry = std::make_shared<State::Entry>();
    entry->value = value;
    entry->text = State::FormatValue(value, style.format);
    entry->style = style;
    entry->mergeKey = mergeKey;
    entry->changedAt = _state->clock;
    entry->intensity = intensity;
    entry->flashIntensity = intensity;
    entry->flashes = _state->quality.flashes;
    PopupStyle animation = style.popup;
    const auto sample = [&](float spread) {
        return std::uniform_real_distribution<float>(-spread, spread)(_state->rng) * intensity;
    };
    animation.motion.velocity += Float2{sample(style.juice.scatterVelocity.x), sample(style.juice.scatterVelocity.y)};
    animation.motion.rotation += sample(style.juice.rotationSpread);
    animation.motion.wobbleRadians *= intensity;
    animation.enter.scale = Float2{1.0f, 1.0f} + (animation.enter.scale - Float2{1.0f, 1.0f}) * intensity;
    animation.enter.scale.x = std::max(0.0f, animation.enter.scale.x);
    animation.enter.scale.y = std::max(0.0f, animation.enter.scale.y);
    animation.enter.rotation *= intensity;
    entry->popup = _state->popups.Emit(
        position, [entry](Painter& paint, const PopupFrame& frame) { State::DrawEntry(paint, frame, *entry); },
        animation);
    if (!entry->popup) return {};
    _state->Prune();
    _state->entries.push_back(entry);
    const auto frame = _state->popups.Frame(entry->popup);
    if (frame && frame->phase != PopupPhase::Delayed) {
        _state->Burst(*entry, frame->position, intensity);
        entry->pendingBurst = false;
    }
    NumberPopupHandle handle;
    handle._popup = entry->popup;
    return handle;
}

bool NumberPopup::Add(NumberPopupHandle handle, NumberPopupValue delta) {
    _state->RequireMutable();
    State::ValidateValue(delta);
    const auto entry = _state->Find(handle._popup);
    return entry && _state->Accumulate(*entry, delta, entry->intensity);
}

bool NumberPopup::Pulse(NumberPopupHandle handle, float intensity) {
    _state->RequireMutable();
    State::ValidateIntensity(intensity);
    const auto entry = _state->Find(handle._popup);
    return entry && _state->Emphasize(*entry, intensity);
}

bool NumberPopup::SetAnchor(NumberPopupHandle handle, Float2 position) {
    _state->RequireMutable();
    return _state->popups.SetAnchor(handle._popup, position);
}

bool NumberPopup::Dismiss(NumberPopupHandle handle) {
    _state->RequireMutable();
    const bool result = _state->popups.Dismiss(handle._popup);
    _state->Prune();
    return result;
}

bool NumberPopup::Cancel(NumberPopupHandle handle) {
    _state->RequireMutable();
    const bool result = _state->popups.Cancel(handle._popup);
    _state->Prune();
    return result;
}

void NumberPopup::SetQuality(const FxQuality& quality) {
    _state->RequireMutable();
    _state->quality = quality;
    _state->particles.SetQuality(quality);
    for (const auto& entry : _state->entries)
        entry->flashes = quality.flashes;
}

void NumberPopup::Update(float deltaSeconds) {
    _state->RequireMutable();
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    _state->clock += deltaSeconds;
    _state->particles.Update(deltaSeconds);
    _state->popups.Update(deltaSeconds);
    _state->Prune();
    for (const auto& entry : _state->entries) {
        if (!entry->pendingBurst) continue;
        const auto frame = _state->popups.Frame(entry->popup);
        if (frame && frame->phase != PopupPhase::Delayed) {
            _state->Burst(*entry, frame->position, entry->intensity);
            entry->pendingBurst = false;
        }
    }
}

void NumberPopup::Draw(Painter& paint) const {
    _state->RequireMutable();
    _state->drawing = true;
    try {
        _state->particles.Draw(paint);
        _state->popups.Draw(paint);
    } catch (...) {
        _state->drawing = false;
        throw;
    }
    _state->drawing = false;
}

void NumberPopup::Clear() {
    _state->RequireMutable();
    _state->popups.Clear();
    _state->particles.Clear();
    _state->entries.clear();
    _state->clock = 0.0;
}

bool NumberPopup::IsAlive(NumberPopupHandle handle) const { return _state->popups.IsAlive(handle._popup); }

std::optional<NumberPopupValue> NumberPopup::Value(NumberPopupHandle handle) const {
    const auto entry = _state->Find(handle._popup);
    return entry ? std::optional{entry->value} : std::nullopt;
}

std::optional<PopupFrame> NumberPopup::Frame(NumberPopupHandle handle) const {
    return _state->popups.Frame(handle._popup);
}

std::size_t NumberPopup::ActiveCount() const { return _state->popups.ActiveCount(); }
