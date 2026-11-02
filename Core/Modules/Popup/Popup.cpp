#include "Modules/Popup/Popup.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <utility>
#include "Graphics/Paint/Painter.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextBlock.h"
#include "Modules/Popup/Internals/State.h"
#include "Utilities/Affine2.h"

namespace {
bool Finite(Float2 value) { return std::isfinite(value.x) && std::isfinite(value.y); }

bool ValidTransition(const PopupTransition& transition) {
    return std::isfinite(transition.seconds) && transition.seconds >= 0.0f && Finite(transition.offset) &&
           Finite(transition.scale) && transition.scale.x >= 0.0f && transition.scale.y >= 0.0f &&
           std::isfinite(transition.rotation) && std::isfinite(transition.opacity) && transition.opacity >= 0.0f &&
           transition.opacity <= 1.0f && transition.easing;
}

bool ValidMotion(const PopupMotion& motion) {
    return Finite(motion.velocity) && Finite(motion.acceleration) && std::isfinite(motion.drag) &&
           motion.drag >= 0.0f && std::isfinite(motion.rotation) && std::isfinite(motion.rotationSpeed) &&
           std::isfinite(motion.wobbleRadians) && std::isfinite(motion.wobbleFrequency) &&
           motion.wobbleFrequency >= 0.0f && std::isfinite(motion.wobbleDamping) && motion.wobbleDamping >= 0.0f;
}

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
} // namespace

PopupDraw PopupText(std::string text, const TextStyle& style) {
    return [text = std::move(text), style, layout = std::optional<TextBlock>{}](Painter& paint,
                                                                                const PopupFrame&) mutable {
        const FontFace& font = paint.Font();
        if (!layout || layout->FontId() != font.Id()) layout = font.Layout(text, style);
        DrawPopupText(paint, font, *layout, style);
    };
}

void DrawPopupText(Painter& paint, const FontFace& font, const TextBlock& block, const TextStyle& style) {
    const Float2 size = block.Extent();
    const float side = style.align == TextAlign::Left ? 0.0f : style.align == TextAlign::Right ? 1.0f : 0.5f;
    paint.Text(font, block, {size.x * (side - 0.5f), -size.y * 0.5f}, style);
}

Popup::Popup(const PopupSettings& settings)
    : _state(std::make_unique<State>(settings)) {}

Popup::~Popup() = default;

PopupStyle Popup::Preset(PopupPreset preset) {
    PopupStyle style;
    switch (preset) {
    case PopupPreset::Pop: break;
    case PopupPreset::Fade:
        style.enter = {.seconds = 0.15f, .opacity = 0.0f};
        style.exit = {.seconds = 0.25f, .opacity = 0.0f};
        style.motion = {};
        break;
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
    }
    return style;
}

bool Popup::IsValid(const PopupStyle& style) {
    const double duration = static_cast<double>(style.enter.seconds) + style.holdSeconds + style.exit.seconds;
    return ValidTransition(style.enter) && ValidTransition(style.exit) && ValidMotion(style.motion) &&
           std::isfinite(style.holdSeconds) && style.holdSeconds >= 0.0f && std::isfinite(style.delaySeconds) &&
           style.delaySeconds >= 0.0f && duration > 0.0 && duration <= std::numeric_limits<float>::max();
}

float Popup::Duration(const PopupStyle& style) { return style.enter.seconds + style.holdSeconds + style.exit.seconds; }

PopupHandle Popup::Emit(Float2 position, PopupDraw draw, const PopupStyle& style) {
    if (!_state->CanMutate()) return {};
    if (!Finite(position) || !draw || !IsValid(style)) {
        _state->Reject("Invalid popup position, callback or style");
        return {};
    }
    if (_state->settings.capacity == 0) return {};
    if (_state->entries.size() == _state->settings.capacity) {
        if (_state->settings.overflow == PopupOverflow::RejectNew) return {};
        const auto oldest =
            std::min_element(_state->entries.begin(), _state->entries.end(),
                             [](const auto& a, const auto& b) { return a.handle._sequence < b.handle._sequence; });
        _state->entries.erase(oldest);
    }
    static std::atomic<std::uint64_t> nextSequence{1};
    PopupHandle handle;
    handle._sequence = nextSequence.fetch_add(1, std::memory_order_relaxed);
    State::Entry entry{.handle = handle,
                       .anchor = position,
                       .draw = std::move(draw),
                       .style = style,
                       .age = -static_cast<double>(style.delaySeconds)};
    const auto at = std::upper_bound(_state->entries.begin(), _state->entries.end(), style.layer,
                                     [](int layer, const State::Entry& item) { return layer < item.style.layer; });
    _state->entries.insert(at, std::move(entry));
    return handle;
}

PopupHandle Popup::Emit(Float2 position, PopupDraw draw, PopupPreset preset) {
    return Emit(position, std::move(draw), Preset(preset));
}

void Popup::Update(float deltaSeconds) {
    if (!_state->CanMutate() || !(deltaSeconds > 0.0f) || !std::isfinite(deltaSeconds)) return;
    for (auto& entry : _state->entries)
        entry.age += deltaSeconds;
    std::erase_if(_state->entries, [](const auto& entry) { return entry.age >= State::Lifetime(entry); });
}

void Popup::Draw(Painter& paint) const {
    if (!_state->CanMutate()) return;
    const DrawingScope drawing(_state->drawing);
    for (const auto& entry : _state->entries) {
        const PopupFrame frame = _state->Sample(entry);
        if (frame.phase == PopupPhase::Delayed || frame.opacity <= 0.0f || frame.scale.x <= 0.0f ||
            frame.scale.y <= 0.0f)
            continue;
        auto transform = paint.Transform(Affine2::Translate(frame.position) * Affine2::Rotate(frame.rotation) *
                                         Affine2::Scale(frame.scale));
        auto opacity = paint.Opacity(frame.opacity);
        entry.draw(paint, frame);
    }
}

void Popup::Clear() {
    if (_state->CanMutate()) _state->entries.clear();
}

bool Popup::SetAnchor(PopupHandle handle, Float2 position) {
    if (!_state->CanMutate()) return false;
    if (!Finite(position)) {
        _state->Reject("Popup position must be finite");
        return false;
    }
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    const Float2 delta = position - entry->anchor;
    entry->anchor = position;
    if (entry->dismissedFrom) entry->dismissedFrom->position += delta;
    return true;
}

bool Popup::Pulse(PopupHandle handle, const PopupPulse& pulse) {
    if (!_state->CanMutate()) return false;
    if (!std::isfinite(pulse.seconds) || pulse.seconds <= 0.0f || !Finite(pulse.scale) ||
        !std::isfinite(pulse.rotation)) {
        _state->Reject("Invalid popup pulse");
        return false;
    }
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    entry->pulse = pulse;
    entry->pulseAt = std::max(0.0, entry->age);
    return true;
}

bool Popup::Extend(PopupHandle handle, float seconds, float maxLifetimeSeconds) {
    if (!_state->CanMutate()) return false;
    if (!std::isfinite(seconds) || seconds < 0.0f || !std::isfinite(maxLifetimeSeconds) || maxLifetimeSeconds <= 0.0f) {
        _state->Reject("Invalid popup lifetime extension");
        return false;
    }
    auto* entry = _state->Find(handle);
    if (!entry || _state->Sample(*entry).phase == PopupPhase::Exit) return false;
    entry->extraHold +=
        std::min(static_cast<double>(seconds), std::max(0.0, maxLifetimeSeconds - State::Lifetime(*entry)));
    return true;
}

bool Popup::Dismiss(PopupHandle handle) {
    if (!_state->CanMutate()) return false;
    auto* entry = _state->Find(handle);
    if (!entry) return false;
    const PopupFrame frame = _state->Sample(*entry);
    if (frame.phase == PopupPhase::Exit) return true;
    if (frame.phase == PopupPhase::Delayed || entry->style.exit.seconds == 0.0f) return Cancel(handle);
    entry->dismissedFrom = frame;
    entry->dismissedAt = entry->age;
    entry->pulseAt = -1.0;
    return true;
}

bool Popup::Cancel(PopupHandle handle) {
    if (!_state->CanMutate()) return false;
    return std::erase_if(_state->entries, [&](const auto& entry) { return entry.handle == handle; }) != 0;
}

bool Popup::IsAlive(PopupHandle handle) const { return _state->Find(handle) != nullptr; }

std::optional<PopupFrame> Popup::Frame(PopupHandle handle) const {
    const auto* entry = _state->Find(handle);
    return entry ? std::optional{_state->Sample(*entry)} : std::nullopt;
}

std::size_t Popup::ActiveCount() const { return _state->entries.size(); }

std::size_t Popup::RejectedCount() const { return _state->rejected; }

const std::string& Popup::FirstRejection() const { return _state->firstRejection; }
