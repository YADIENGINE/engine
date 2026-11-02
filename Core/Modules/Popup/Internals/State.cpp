#include "Modules/Popup/Internals/State.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include "Utilities/MathUtils.h"

namespace {
// Closed form of dv/dt = acceleration - drag * v, so update subdivision never changes the path.
Float2 Travel(const PopupMotion& motion, float age) {
    if (motion.drag <= 0.0f) return motion.velocity * age + motion.acceleration * (0.5f * age * age);
    const Float2 terminal = motion.acceleration * (1.0f / motion.drag);
    return terminal * age + (motion.velocity - terminal) * (-std::expm1(-motion.drag * age) / motion.drag);
}

float Rotation(const PopupMotion& motion, float age) {
    return motion.rotation + motion.rotationSpeed * age +
           motion.wobbleRadians * std::sin(2.0f * Pi * motion.wobbleFrequency * age) *
               std::exp(-motion.wobbleDamping * age);
}
} // namespace

Popup::State::State(const PopupSettings& options)
    : settings(options) {
    entries.reserve(options.capacity);
}

bool Popup::State::CanMutate() const {
    if (!drawing) return true;
    if (settings.strict) throw std::logic_error("Cannot mutate or reenter Popup during Draw");
    Reject("Popup mutated or reentered during Draw");
    return false;
}

void Popup::State::Reject(std::string_view message) const {
    if (settings.strict) throw std::invalid_argument(std::string(message));
    if (rejected == 0) firstRejection = message;
    ++rejected;
}

// A broken custom easing falls back to linear instead of poisoning the pose.
float Popup::State::Curve(const PopupTransition& transition, float progress) const {
    const float clamped = Clamp01(progress);
    const float value = transition.easing(clamped);
    if (std::isfinite(value)) return value;
    Reject("Popup easing returned a non-finite value");
    return clamped;
}

Popup::State::Entry* Popup::State::Find(PopupHandle handle) {
    const auto found =
        std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.handle == handle; });
    return found == entries.end() ? nullptr : &*found;
}

const Popup::State::Entry* Popup::State::Find(PopupHandle handle) const {
    const auto found =
        std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) { return entry.handle == handle; });
    return found == entries.end() ? nullptr : &*found;
}

double Popup::State::Lifetime(const Entry& entry) {
    if (entry.dismissedFrom) return entry.dismissedAt + entry.style.exit.seconds;
    return static_cast<double>(entry.style.enter.seconds) + entry.style.holdSeconds + entry.extraHold +
           entry.style.exit.seconds;
}

PopupFrame Popup::State::Sample(const Entry& entry) const {
    PopupFrame frame;
    frame.position = entry.anchor;
    if (entry.age < 0.0) return frame;
    const float age = static_cast<float>(entry.age);
    frame.ageSeconds = age;
    frame.progress = Clamp01(static_cast<float>(entry.age / Lifetime(entry)));
    frame.position += Travel(entry.style.motion, age);
    frame.rotation = Rotation(entry.style.motion, age);
    frame.opacity = 1.0f;
    const double exitAt = entry.style.enter.seconds + static_cast<double>(entry.style.holdSeconds) + entry.extraHold;
    if (entry.dismissedFrom) {
        frame.phase = PopupPhase::Exit;
        frame.phaseProgress = Clamp01(static_cast<float>((entry.age - entry.dismissedAt) / entry.style.exit.seconds));
        const float curve = Curve(entry.style.exit, frame.phaseProgress);
        const PopupFrame& from = *entry.dismissedFrom;
        frame.position =
            from.position +
            (Travel(entry.style.motion, age) - Travel(entry.style.motion, static_cast<float>(entry.dismissedAt))) +
            entry.style.exit.offset * curve;
        frame.scale = Mix(from.scale, entry.style.exit.scale, curve);
        frame.rotation += Mix(from.rotation - Rotation(entry.style.motion, static_cast<float>(entry.dismissedAt)),
                              entry.style.exit.rotation, curve);
        frame.opacity = Mix(from.opacity, entry.style.exit.opacity, Clamp01(curve));
    } else if (entry.age < entry.style.enter.seconds) {
        frame.phase = PopupPhase::Enter;
        frame.phaseProgress = age / entry.style.enter.seconds;
        const float curve = Curve(entry.style.enter, frame.phaseProgress);
        frame.position += entry.style.enter.offset * (1.0f - curve);
        frame.scale = Mix(entry.style.enter.scale, Float2{1.0f, 1.0f}, curve);
        frame.rotation += entry.style.enter.rotation * (1.0f - curve);
        frame.opacity = Mix(entry.style.enter.opacity, 1.0f, Clamp01(curve));
    } else if (entry.age < exitAt || entry.style.exit.seconds == 0.0f) {
        frame.phase = PopupPhase::Hold;
        const double hold = entry.style.holdSeconds + entry.extraHold;
        frame.phaseProgress = hold > 0.0 ? static_cast<float>((entry.age - entry.style.enter.seconds) / hold) : 1.0f;
    } else {
        frame.phase = PopupPhase::Exit;
        frame.phaseProgress = Clamp01(static_cast<float>((entry.age - exitAt) / entry.style.exit.seconds));
        const float curve = Curve(entry.style.exit, frame.phaseProgress);
        frame.position += entry.style.exit.offset * curve;
        frame.scale = Mix(Float2{1.0f, 1.0f}, entry.style.exit.scale, curve);
        frame.rotation += entry.style.exit.rotation * curve;
        frame.opacity = Mix(1.0f, entry.style.exit.opacity, Clamp01(curve));
    }
    if (entry.pulseAt >= 0.0 && entry.age >= entry.pulseAt && entry.pulse.seconds > 0.0f) {
        const float strength =
            1.0f - EaseOutCubic(static_cast<float>((entry.age - entry.pulseAt) / entry.pulse.seconds));
        frame.scale.x *= 1.0f + entry.pulse.scale.x * strength;
        frame.scale.y *= 1.0f + entry.pulse.scale.y * strength;
        frame.rotation += entry.pulse.rotation * strength;
    }
    // Overshooting curves are useful for motion and scale, but opacity and mirrored geometry stay bounded.
    frame.scale.x = std::max(0.0f, frame.scale.x);
    frame.scale.y = std::max(0.0f, frame.scale.y);
    frame.opacity = Clamp01(frame.opacity);
    return frame;
}
