#pragma once
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <variant>
#include "Fx/FxQuality.h"
#include "Fx/Particles/ParticleSystem.h"
#include "Graphics/Text/TextStyle.h"
#include "Modules/Popup/Popup.h"
#include "Diagnostics/DevelopmentChecks.h"
#include "Utilities/Math/Scalar.h"

class Painter;

// Pop comes first: value-initialized, it equals the default NumberPopupStyle.
enum class NumberPopupPreset { Pop, Float, Burst, Heavy };
enum class NumberPopupSign { NegativeOnly, Always, None };

// Integral input retains signed 64-bit precision. Floating input (including float) uses double.
using NumberPopupValue = std::variant<std::int64_t, double>;

class NumberPopupHandle {
public:
    NumberPopupHandle() = default;
    [[nodiscard]] explicit operator bool() const { return static_cast<bool>(_popup); }
    friend bool operator==(const NumberPopupHandle&, const NumberPopupHandle&) = default;

private:
    friend class NumberPopup;
    PopupHandle _popup;
};

// Zero means independent emission. A nonzero key identifies one compatible, caller-defined value stream.
struct NumberPopupMergeKey {
    std::uint64_t value = 0;
    // A nonzero key from integers and enums, e.g. Of(ScoreGroup::Food, playerIndex). Order matters.
    template <class... Parts>
        requires(sizeof...(Parts) > 0) && ((std::is_integral_v<Parts> || std::is_enum_v<Parts>) && ...)
    [[nodiscard]] static constexpr NumberPopupMergeKey Of(Parts... parts) {
        constexpr std::uint64_t Prime = 1099511628211ull;
        std::uint64_t hash = 14695981039346656037ull;
        ((hash = (hash ^ static_cast<std::uint64_t>(parts)) * Prime), ...);
        return {hash == 0 ? 1 : hash};
    }
    friend bool operator==(NumberPopupMergeKey, NumberPopupMergeKey) = default;
};

struct NumberPopupFormat {
    NumberPopupSign sign = NumberPopupSign::NegativeOnly;
    int decimals = 0;     // 0..9; fixed precision, also applied to integral input
    bool compact = false; // K, M, B, T
    bool trimTrailingZeros = false;
    std::string prefix;
    std::string suffix;
    // Optional complete-label override. Called only on emission/value changes, never during Draw.
    std::function<std::string(const NumberPopupValue&)> formatter;
};

struct NumberPopupJuice {
    Float2 scatterVelocity{20.0f, 0.0f}; // Independent +/- spread added to launch velocity
    float rotationSpread = 0.08f;        // +/- clockwise radians
    float flashSeconds = 0.06f;
    Color4 flashColor{255, 255, 255, 255};
    int particleCount = 0;
    ParticleEmission particles{.speed = {60.0f, 140.0f},
                               .lifetime = {0.15f, 0.3f},
                               .size = {1.5f, 3.0f},
                               .rotation = {-Pi, Pi},
                               .spin = {-4.0f, 4.0f},
                               .acceleration = {0.0f, 120.0f},
                               .drag = 2.0f};
    ParticleAppearance particleLook{.shape = ParticleShape::Star,
                                    .blend = ParticleBlend::Additive,
                                    .colors = ParticleFadeOut(0.8f),
                                    .size = {.start = 1.0f, .end = 0.2f},
                                    .sparkTrailSeconds = 0.035f,
                                    .sparkThickness = 0.5f};
    std::optional<Color4> particleColor; // Empty: the text's colour
    PopupPulse pulse;
};

struct NumberPopupMerge {
    float windowSeconds = 0.12f;     // Measured since the most recent accepted value change; zero disables auto-merge
    float extendSeconds = 0.12f;     // Hold time each merge or Add gains, up to maxLifetimeSeconds
    float maxLifetimeSeconds = 1.5f; // Caps extensions; never shortens the original style's lifetime
    bool followAnchor = false;       // A merge moves the entry to the incoming position (moving targets)
};

struct NumberPopupStyle {
    PopupStyle popup;
    TextStyle text{.size = 28.0f,
                   .align = TextAlign::Center,
                   .outline = {1.5f, {15, 15, 25, 220}},
                   .shadow = {{1.0f, 2.0f}, {0, 0, 0, 140}}};
    NumberPopupFormat format;
    NumberPopupJuice juice;
    NumberPopupMerge merge;
};

// Per-emission choices beside the reusable style.
struct NumberPopupEmit {
    float intensity = 1.0f; // 0..4; launch variation, squash/rotation, flash, particles; never the numeric value
    NumberPopupMergeKey mergeKey;
    std::optional<Color4> color; // Replaces style.text.color for this entry
};

struct NumberPopupSettings {
    std::size_t capacity = 128;
    PopupOverflow overflow = PopupOverflow::ReplaceOldest;
    std::size_t particleCapacity = 1024;
    std::optional<std::uint32_t> seed;
    // Invalid input: strict throws at the call; otherwise the call is skipped and counted (RejectedCount).
    bool strict = DevelopmentChecks();
};

// Owns its Popup and local particle system. Main-thread only; no automatic scene, audio, camera or theme access.
class NumberPopup final {
public:
    explicit NumberPopup(const NumberPopupSettings& settings = {});
    ~NumberPopup();
    [[nodiscard]] static NumberPopupStyle Preset(NumberPopupPreset preset);
    // Decimals outside 0..9 are clamped here; Emit treats them as invalid input.
    [[nodiscard]] static std::string Format(NumberPopupValue value, const NumberPopupFormat& format = {});
    // Equal nonzero keys merge before Exit, within the first entry's window, and only for compatible signs.
    // The first entry's anchor (unless merge.followAnchor)/style are retained. Empty handle on capacity rejection or
    // skipped invalid input.
    NumberPopupHandle Emit(Float2 position, NumberPopupValue value, const NumberPopupStyle& style = {},
                           const NumberPopupEmit& options = {});
    NumberPopupHandle Emit(Float2 position, NumberPopupValue value, NumberPopupPreset preset,
                           const NumberPopupEmit& options = {});
    // Text with the same animation and juice ("CRIT!"). Style format and options.mergeKey are not used; it has no
    // Value and Add returns false for it.
    NumberPopupHandle EmitLabel(Float2 position, std::string label, const NumberPopupStyle& style = {},
                                const NumberPopupEmit& options = {});
    NumberPopupHandle EmitLabel(Float2 position, std::string label, NumberPopupPreset preset,
                                const NumberPopupEmit& options = {});
    // Adds exactly for integral + integral, saturating at the limits; mixed values use double. False during Exit/if
    // stale.
    bool Add(NumberPopupHandle handle, NumberPopupValue delta);
    bool Pulse(NumberPopupHandle handle, float intensity = 1.0f);
    bool SetAnchor(NumberPopupHandle handle, Float2 position);
    bool Dismiss(NumberPopupHandle handle);
    bool Cancel(NumberPopupHandle handle);
    void SetQuality(const FxQuality& quality);
    void Update(float deltaSeconds);
    // Local particles behind centered text; uses Painter::Font. Caller controls the coordinate space and layer.
    void Draw(Painter& paint) const;
    void Clear();
    [[nodiscard]] bool IsAlive(NumberPopupHandle handle) const;
    [[nodiscard]] std::optional<NumberPopupValue> Value(NumberPopupHandle handle) const;
    [[nodiscard]] std::optional<PopupFrame> Frame(NumberPopupHandle handle) const;
    // Number entries, including delayed entries; particles may briefly outlive them.
    [[nodiscard]] std::size_t ActiveCount() const;
    // Live local particles, after FxQuality.
    [[nodiscard]] std::size_t ParticleCount() const;
    // Calls skipped since construction because of invalid input (non-strict only), and the first one's reason.
    [[nodiscard]] std::size_t RejectedCount() const;
    [[nodiscard]] const std::string& FirstRejection() const;

private:
    class State;
    std::unique_ptr<State> _state;

public:
    NumberPopup(const NumberPopup&) = delete;
    NumberPopup& operator=(const NumberPopup&) = delete;
    NumberPopup(NumberPopup&&) = delete;
    NumberPopup& operator=(NumberPopup&&) = delete;
};
