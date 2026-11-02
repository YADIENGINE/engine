#include "Modules/Popup/Popup.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include "Graphics/Paint/Painter.h"
#include "Modules/Popup/Internals/State.h"
#include "Utilities/Affine2.h"

namespace {
void ValidateTransition(const PopupTransition& transition) {
    const auto finite = [](Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); };
    if (!std::isfinite(transition.seconds) || transition.seconds < 0.0f || !finite(transition.offset) ||
        !finite(transition.scale) || transition.scale.x < 0.0f || transition.scale.y < 0.0f ||
        !std::isfinite(transition.rotation) || !std::isfinite(transition.opacity) || transition.opacity < 0.0f ||
        transition.opacity > 1.0f || !transition.easing)
        throw std::invalid_argument("Invalid popup transition");
}
} // namespace

Popup::Popup(const PopupSettings& settings)
    : _state(std::make_unique<State>(settings)) {}

Popup::~Popup() = default;

PopupStyle Popup::Preset(PopupPreset preset) {
    PopupStyle style;
    switch (preset) {
    case PopupPreset::Fade:
        style.enter = {.seconds = 0.15f, .opacity = 0.0f};
        style.exit = {.seconds = 0.25f, .opacity = 0.0f};
        style.motion = {};
        break;
    case PopupPreset::Pop: break;
    case PopupPreset::Float:
        style.enter = {.seconds = 0.1f, .opacity = 0.0f};
        style.holdSeconds = 0.6f;
        style.motion.velocity = {0.0f, -65.0f};
        break;
    case PopupPreset::Bounce:
        style.enter.scale = {0.45f, 1.25f};
        style.motion.velocity = {0.0f, -150.0f};
        style.motion.acceleration = {0.0f, 240.0f};
        break;
    default: throw std::invalid_argument("Invalid popup preset");
    }
    return style;
}

float Popup::Duration(const PopupStyle& style) {
    ValidateTransition(style.enter);
    ValidateTransition(style.exit);
    const PopupMotion& motion = style.motion;
    State::ValidatePosition(motion.velocity);
    State::ValidatePosition(motion.acceleration);
    const double duration = static_cast<double>(style.enter.seconds) + style.holdSeconds + style.exit.seconds;
    if (!std::isfinite(style.holdSeconds) || style.holdSeconds < 0.0f || !std::isfinite(style.delaySeconds) ||
        style.delaySeconds < 0.0f || !(duration > 0.0) || duration > std::numeric_limits<float>::max() ||
        !std::isfinite(motion.rotation) || !std::isfinite(motion.rotationSpeed) ||
        !std::isfinite(motion.wobbleRadians) || !std::isfinite(motion.wobbleFrequency) ||
        motion.wobbleFrequency < 0.0f || !std::isfinite(motion.wobbleDamping) || motion.wobbleDamping < 0.0f)
        throw std::invalid_argument("Invalid popup style");
    return static_cast<float>(duration);
}

PopupHandle Popup::Emit(Float2 position, PopupDraw draw, const PopupStyle& style) {
    _state->RequireMutable();
    State::ValidatePosition(position);
    (void)Duration(style);
    if (!draw) throw std::invalid_argument("Popup draw callback cannot be empty");
    if (_state->settings.capacity == 0) return {};
    if (_state->entries.size() == _state->settings.capacity) {
        if (_state->settings.overflow == PopupOverflow::RejectNew) return {};
        const auto oldest = std::min_element(_state->entries.begin(), _state->entries.end(),
                                             [](const auto& a, const auto& b) { return a.order < b.order; });
        _state->entries.erase(oldest);
    }
    if (_state->nextSequence == 0) throw std::overflow_error("Popup handle sequence exhausted");
    PopupHandle handle;
    handle._owner = _state->owner;
    handle._sequence = _state->nextSequence++;
    State::Entry entry{.handle = handle,
                       .anchor = position,
                       .draw = std::move(draw),
                       .style = style,
                       .age = -static_cast<double>(style.delaySeconds),
                       .order = handle._sequence};
    const auto at = std::upper_bound(_state->entries.begin(), _state->entries.end(), style.layer,
                                     [](int layer, const State::Entry& item) { return layer < item.style.layer; });
    _state->entries.insert(at, std::move(entry));
    return handle;
}

void Popup::Update(float deltaSeconds) {
    _state->RequireMutable();
    if (!(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (auto& entry : _state->entries)
        entry.age += deltaSeconds;
    std::erase_if(_state->entries, [](const auto& entry) { return entry.age >= State::Lifetime(entry); });
}

void Popup::Draw(Painter& paint) const {
    _state->RequireMutable();
    _state->drawing = true;
    try {
        for (const auto& entry : _state->entries) {
            const PopupFrame frame = State::Sample(entry);
            if (frame.phase == PopupPhase::Delayed || frame.opacity <= 0.0f || frame.scale.x <= 0.0f ||
                frame.scale.y <= 0.0f)
                continue;
            auto transform = paint.Transform(Affine2::Translate(frame.position) * Affine2::Rotate(frame.rotation) *
                                             Affine2::Scale(frame.scale));
            auto opacity = paint.Opacity(frame.opacity);
            entry.draw(paint, frame);
        }
    } catch (...) {
        _state->drawing = false;
        throw;
    }
    _state->drawing = false;
}

void Popup::Clear() {
    _state->RequireMutable();
    _state->entries.clear();
}

bool Popup::SetAnchor(PopupHandle handle, Float2 position) {
    _state->RequireMutable();
    State::ValidatePosition(position);
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    const Float2 delta = position - entry->anchor;
    entry->anchor = position;
    if (entry->dismissedFrom) entry->dismissedFrom->position += delta;
    return true;
}

bool Popup::Pulse(PopupHandle handle, const PopupPulse& pulse) {
    _state->RequireMutable();
    if (!std::isfinite(pulse.seconds) || pulse.seconds <= 0.0f || !std::isfinite(pulse.scale.x) ||
        !std::isfinite(pulse.scale.y) || pulse.scale.x < 0.0f || pulse.scale.y < 0.0f || !std::isfinite(pulse.rotation))
        throw std::invalid_argument("Invalid popup pulse");
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    entry->pulse = pulse;
    entry->pulseAt = std::max(0.0, entry->age);
    return true;
}

bool Popup::Extend(PopupHandle handle, float seconds, float maxLifetimeSeconds) {
    _state->RequireMutable();
    if (!std::isfinite(seconds) || seconds < 0.0f || !std::isfinite(maxLifetimeSeconds) || maxLifetimeSeconds <= 0.0f)
        throw std::invalid_argument("Invalid popup lifetime extension");
    auto* entry = _state->Find(handle);
    if (!entry || State::Sample(*entry).phase == PopupPhase::Exit) return false;
    entry->extraHold +=
        std::min(static_cast<double>(seconds), std::max(0.0, maxLifetimeSeconds - State::Lifetime(*entry)));
    return true;
}

bool Popup::Dismiss(PopupHandle handle) {
    _state->RequireMutable();
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    const PopupFrame frame = State::Sample(*entry);
    if (frame.phase == PopupPhase::Exit) return true;
    if (frame.phase == PopupPhase::Delayed || entry->style.exit.seconds == 0.0f) return Cancel(handle);
    entry->dismissedFrom = frame;
    entry->dismissedAt = entry->age;
    entry->pulseAt = -1.0;
    return true;
}

bool Popup::Cancel(PopupHandle handle) {
    _state->RequireMutable();
    return std::erase_if(_state->entries, [&](const auto& entry) { return entry.handle == handle; }) != 0;
}

bool Popup::IsAlive(PopupHandle handle) const { return _state->Find(handle) != nullptr; }

std::optional<PopupFrame> Popup::Frame(PopupHandle handle) const {
    const auto* entry = _state->Find(handle);
    return entry ? std::optional{State::Sample(*entry)} : std::nullopt;
}

std::size_t Popup::ActiveCount() const { return _state->entries.size(); }
