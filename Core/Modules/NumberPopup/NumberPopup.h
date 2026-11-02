#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include "Fx/FxQuality.h"
#include "Graphics/Draw/ParticleDraw.h"
#include "Graphics/Text/TextStyle.h"
#include "Modules/Popup/Popup.h"
#include "Utilities/FloatRange.h"

class Painter;

enum class NumberPopupPreset { Float, Pop, Burst, Heavy };
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
    FloatRange particleSpeed{60.0f, 140.0f};
    FloatRange particleLifetime{0.15f, 0.3f};
    FloatRange particleSize{1.5f, 3.0f};
    Float2 particleAcceleration{0.0f, 120.0f};
    ParticleShape particleShape = ParticleShape::Star;
    PopupPulse pulse;
};

struct NumberPopupMerge {
    float windowSeconds = 0.12f;     // Measured since the most recent accepted value change; zero disables auto-merge
    float maxLifetimeSeconds = 1.5f; // Caps extensions; never shortens the original style's lifetime
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

struct NumberPopupSettings {
    std::size_t capacity = 128;
    PopupOverflow overflow = PopupOverflow::ReplaceOldest;
    std::size_t particleCapacity = 1024;
    std::optional<std::uint32_t> seed;
};

// Owns its Popup and local particle system. Main-thread only; no automatic scene, audio, camera or theme access.
class NumberPopup final {
public:
    explicit NumberPopup(const NumberPopupSettings& settings = {});
    ~NumberPopup();
    [[nodiscard]] static NumberPopupStyle Preset(NumberPopupPreset preset);
    [[nodiscard]] static std::string Format(NumberPopupValue value, const NumberPopupFormat& format = {});
    // Intensity 0..4 scales launch variation, squash/rotation, flash and particles, never the numeric value.
    // Equal nonzero keys merge before Exit, within the first entry's window, and only for compatible signs.
    // The first entry's anchor/style are retained. Empty handle on capacity rejection.
    NumberPopupHandle Emit(Float2 position, NumberPopupValue value, const NumberPopupStyle& style = {},
                           float intensity = 1.0f, NumberPopupMergeKey mergeKey = {});
    // Adds exactly for integral + integral (overflow throws); mixed values use double. False during Exit/if stale.
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

private:
    class State;
    std::unique_ptr<State> _state;

public:
    NumberPopup(const NumberPopup&) = delete;
    NumberPopup& operator=(const NumberPopup&) = delete;
    NumberPopup(NumberPopup&&) = delete;
    NumberPopup& operator=(NumberPopup&&) = delete;
};
