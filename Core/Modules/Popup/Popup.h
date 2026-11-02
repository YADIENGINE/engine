#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include "Graphics/Anim/Easing.h"
#include "Utilities/Float2.h"

class Painter;

enum class PopupPreset { Fade, Pop, Float, Bounce };
enum class PopupPhase { Delayed, Enter, Hold, Exit };
enum class PopupOverflow { RejectNew, ReplaceOldest };

// Opaque identity, local to its owning Popup. A nonempty handle may already have expired: use IsAlive.
class PopupHandle {
public:
    PopupHandle() = default;
    [[nodiscard]] explicit operator bool() const { return _sequence != 0; }
    friend bool operator==(const PopupHandle&, const PopupHandle&) = default;

private:
    friend class Popup;
    std::shared_ptr<const std::byte> _owner;
    std::uint64_t _sequence = 0;
};

// Endpoint relative to the normal pose. Enter interpolates endpoint -> normal; Exit normal -> endpoint.
struct PopupTransition {
    float seconds = 0.0f;
    Float2 offset{};
    Float2 scale{1.0f, 1.0f};
    float rotation = 0.0f; // Clockwise radians
    float opacity = 1.0f;
    float (*easing)(float) = EaseOutCubic;
};

struct PopupMotion {
    Float2 velocity{};
    Float2 acceleration{};
    float rotation = 0.0f;
    float rotationSpeed = 0.0f; // Radians / second
    float wobbleRadians = 0.0f;
    float wobbleFrequency = 0.0f; // Cycles / second
    float wobbleDamping = 0.0f;   // Exponential decay / second
};

struct PopupStyle {
    PopupTransition enter{.seconds = 0.12f, .scale = {0.6f, 0.6f}, .opacity = 0.0f, .easing = EaseOutBack};
    float holdSeconds = 0.4f;
    PopupTransition exit{.seconds = 0.25f, .offset = {0.0f, -12.0f}, .opacity = 0.0f};
    PopupMotion motion{.velocity = {0.0f, -40.0f}};
    float delaySeconds = 0.0f;
    int layer = 0; // Lower first; emission order within a layer
};

struct PopupSettings {
    std::size_t capacity = 128; // Zero disables emission
    PopupOverflow overflow = PopupOverflow::ReplaceOldest;
};

// A bounded, decaying overlay. Retriggering replaces the pulse, never accumulates unbounded scale.
struct PopupPulse {
    float seconds = 0.18f;
    Float2 scale{0.18f, 0.18f}; // Additional fraction of the current scale
    float rotation = 0.0f;
};

// Owned snapshot in the caller's coordinate space. Pose/opacity are already applied to the callback's Painter.
struct PopupFrame {
    PopupPhase phase = PopupPhase::Delayed;
    float ageSeconds = 0.0f; // Zero while delayed
    float progress = 0.0f;
    float phaseProgress = 0.0f;
    Float2 position{};
    Float2 scale{1.0f, 1.0f};
    float rotation = 0.0f;
    float opacity = 0.0f;
};

using PopupDraw = std::function<void(Painter&, const PopupFrame&)>;

// Explicitly updated, main-thread popup collection. Owns callback captures, never a Painter or captured resources.
class Popup final {
public:
    explicit Popup(const PopupSettings& settings = {});
    ~Popup();
    [[nodiscard]] static PopupStyle Preset(PopupPreset preset);
    // Validates the style; returns active lifetime, excluding delay. Invalid arguments throw invalid_argument.
    [[nodiscard]] static float Duration(const PopupStyle& style);
    // Callback draws around local (0, 0); cannot mutate/reenter this collection. Empty handle on capacity rejection.
    // Captured references and texture handles must outlive the entry. Style and callback are owned snapshots.
    PopupHandle Emit(Float2 position, PopupDraw draw, const PopupStyle& style = {});
    void Update(float deltaSeconds);
    void Draw(Painter& paint) const;
    void Clear();
    bool SetAnchor(PopupHandle handle, Float2 position);
    bool Pulse(PopupHandle handle, const PopupPulse& pulse = {});
    // Adds hold time before Exit, capped by total active lifetime. Never rewinds Enter or Exit.
    bool Extend(PopupHandle handle, float seconds, float maxLifetimeSeconds);
    // Exit from the currently sampled pose; delayed entries and zero-duration exits are removed immediately.
    bool Dismiss(PopupHandle handle);
    bool Cancel(PopupHandle handle);
    [[nodiscard]] bool IsAlive(PopupHandle handle) const;
    [[nodiscard]] std::optional<PopupFrame> Frame(PopupHandle handle) const;
    [[nodiscard]] std::size_t ActiveCount() const;

private:
    class State;
    std::unique_ptr<State> _state;

public:
    Popup(const Popup&) = delete;
    Popup& operator=(const Popup&) = delete;
    Popup(Popup&&) = delete;
    Popup& operator=(Popup&&) = delete;
};
