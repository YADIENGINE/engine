#include "Modules/NumberPopup/NumberPopup.h"
#include <algorithm>
#include <cmath>
#include <utility>
#include "Modules/NumberPopup/Internals/State.h"

namespace {
// Clears the flag on every exit from Draw, including a throwing callback.
class DrawingScope {
public:
    explicit DrawingScope(bool& flag)
        : _flag(flag) {
        _flag = true;
    }
    ~DrawingScope() { _flag = false; }

private:
    bool& _flag;

public:
    DrawingScope(const DrawingScope&) = delete;
    DrawingScope& operator=(const DrawingScope&) = delete;
};

bool ValidPosition(Float2 position) { return std::isfinite(position.x) && std::isfinite(position.y); }
} // namespace

NumberPopup::NumberPopup(const NumberPopupSettings& settings)
    : _state(std::make_unique<State>(settings)) {}

NumberPopup::~NumberPopup() = default;

NumberPopupStyle NumberPopup::Preset(NumberPopupPreset preset) {
    NumberPopupStyle style;
    switch (preset) {
    case NumberPopupPreset::Pop: break;
    case NumberPopupPreset::Float:
        style.popup = Popup::Preset(PopupPreset::Float);
        style.juice.scatterVelocity = {};
        style.juice.rotationSpread = 0.0f;
        style.juice.flashSeconds = 0.0f;
        break;
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
        style.juice.particles.speed = {100.0f, 220.0f};
        style.juice.flashSeconds = 0.1f;
        style.juice.pulse.scale = {0.3f, 0.3f};
        style.text.size = 42.0f;
        style.text.outline.width = 2.0f;
        break;
    }
    return style;
}

std::string NumberPopup::Format(NumberPopupValue value, const NumberPopupFormat& format) {
    return State::FormatValue(value, format);
}

NumberPopupHandle NumberPopup::Emit(Float2 position, NumberPopupValue value, const NumberPopupStyle& style,
                                    const NumberPopupEmit& options) {
    if (!_state->CanMutate()) return {};
    if (!ValidPosition(position) || !State::ValidValue(value) || !State::ValidStyle(style) ||
        !State::ValidIntensity(options.intensity)) {
        _state->Reject("Invalid number popup position, value, style or intensity");
        return {};
    }
    if (options.mergeKey.value != 0 && style.merge.windowSeconds > 0.0f) {
        const auto sign = [](const NumberPopupValue& number) {
            return std::visit([](auto v) { return (v > 0) - (v < 0); }, number);
        };
        for (auto at = _state->entries.rbegin(); at != _state->entries.rend(); ++at) {
            auto& entry = **at;
            if (entry.mergeKey != options.mergeKey || entry.style.merge.windowSeconds == 0.0f ||
                _state->clock - entry.changedAt > entry.style.merge.windowSeconds ||
                sign(*entry.value) * sign(value) < 0)
                continue;
            if (_state->Accumulate(entry, value, options.intensity)) {
                if (entry.style.merge.followAnchor) _state->popups.SetAnchor(entry.popup, position);
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
    if (options.color) entry->style.text.color = *options.color;
    entry->mergeKey = options.mergeKey;
    NumberPopupHandle handle;
    handle._popup = _state->Launch(position, entry, options);
    return handle;
}

NumberPopupHandle NumberPopup::Emit(Float2 position, NumberPopupValue value, NumberPopupPreset preset,
                                    const NumberPopupEmit& options) {
    return Emit(position, value, Preset(preset), options);
}

NumberPopupHandle NumberPopup::EmitLabel(Float2 position, std::string label, const NumberPopupStyle& style,
                                         const NumberPopupEmit& options) {
    if (!_state->CanMutate()) return {};
    if (!ValidPosition(position) || !State::ValidStyle(style) || !State::ValidIntensity(options.intensity)) {
        _state->Reject("Invalid number popup position, style or intensity");
        return {};
    }
    auto entry = std::make_shared<State::Entry>();
    entry->text = std::move(label);
    entry->style = style;
    if (options.color) entry->style.text.color = *options.color;
    NumberPopupHandle handle;
    handle._popup = _state->Launch(position, entry, options);
    return handle;
}

NumberPopupHandle NumberPopup::EmitLabel(Float2 position, std::string label, NumberPopupPreset preset,
                                         const NumberPopupEmit& options) {
    return EmitLabel(position, std::move(label), Preset(preset), options);
}

bool NumberPopup::Add(NumberPopupHandle handle, NumberPopupValue delta) {
    if (!_state->CanMutate()) return false;
    if (!State::ValidValue(delta)) {
        _state->Reject("Number popup value must be finite");
        return false;
    }
    const auto entry = _state->Find(handle._popup);
    return entry && _state->Accumulate(*entry, delta, entry->intensity);
}

bool NumberPopup::Pulse(NumberPopupHandle handle, float intensity) {
    if (!_state->CanMutate()) return false;
    if (!State::ValidIntensity(intensity)) {
        _state->Reject("Number popup intensity must be between 0 and 4");
        return false;
    }
    const auto entry = _state->Find(handle._popup);
    return entry && _state->Emphasize(*entry, intensity);
}

bool NumberPopup::SetAnchor(NumberPopupHandle handle, Float2 position) {
    return _state->CanMutate() && _state->popups.SetAnchor(handle._popup, position);
}

bool NumberPopup::Dismiss(NumberPopupHandle handle) {
    if (!_state->CanMutate()) return false;
    const bool result = _state->popups.Dismiss(handle._popup);
    _state->Prune();
    return result;
}

bool NumberPopup::Cancel(NumberPopupHandle handle) {
    if (!_state->CanMutate()) return false;
    const bool result = _state->popups.Cancel(handle._popup);
    _state->Prune();
    return result;
}

void NumberPopup::SetQuality(const FxQuality& quality) {
    if (!_state->CanMutate()) return;
    _state->quality = quality;
    _state->particles.SetQuality(quality);
    for (const auto& entry : _state->entries)
        entry->flashes = quality.flashes;
}

void NumberPopup::Update(float deltaSeconds) {
    if (!_state->CanMutate() || !(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
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
    if (!_state->CanMutate()) return;
    const DrawingScope drawing(_state->drawing);
    _state->particles.Draw(paint);
    _state->popups.Draw(paint);
}

void NumberPopup::Clear() {
    if (!_state->CanMutate()) return;
    _state->popups.Clear();
    _state->particles.Clear();
    _state->entries.clear();
    _state->clock = 0.0;
}

bool NumberPopup::IsAlive(NumberPopupHandle handle) const { return _state->popups.IsAlive(handle._popup); }

std::optional<NumberPopupValue> NumberPopup::Value(NumberPopupHandle handle) const {
    const auto entry = _state->Find(handle._popup);
    return entry ? entry->value : std::nullopt;
}

std::optional<PopupFrame> NumberPopup::Frame(NumberPopupHandle handle) const {
    return _state->popups.Frame(handle._popup);
}

std::size_t NumberPopup::ActiveCount() const { return _state->popups.ActiveCount(); }

std::size_t NumberPopup::ParticleCount() const { return _state->particles.ActiveCount(); }

std::size_t NumberPopup::RejectedCount() const { return _state->rejected + _state->popups.RejectedCount(); }

const std::string& NumberPopup::FirstRejection() const {
    return _state->rejected > 0 ? _state->firstRejection : _state->popups.FirstRejection();
}
