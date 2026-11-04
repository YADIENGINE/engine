#include "Ui/Widgets/UiSlider.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "Ui/Looks/UiSliderLook.h"
#include "Ui/Widgets/Internals/UiControlRow.h"
#include "Ui/Widgets/UiTooltip.h"

namespace {
constexpr float DefaultStepShare = 0.05f;

// A press drags the slider only when it began on the slider, not on the row's label.
struct Drag {
    bool held = false;
    bool onTrack = false;
};

[[nodiscard]] float Snapped(float value, float min, float max, float step) {
    return std::clamp(min + std::round((value - min) / step) * step, min, max);
}
[[nodiscard]] std::string_view NumberText(Ui& ui, float value, int decimals) {
    std::array<char, 32> buffer{};
    const int length = std::snprintf(buffer.data(), buffer.size(), "%.*f", decimals, static_cast<double>(value));
    return ui.Text({buffer.data(), static_cast<std::size_t>(std::max(length, 0))});
}

[[nodiscard]] bool RunSlider(Ui& ui, UiId id, std::string_view label, float& value, float min, float max, float stride,
                             int decimals, const UiControlOptions& options) {
    if (!(max > min) || !(stride > 0.0f)) {
        throw std::invalid_argument("UiSlider: needs max above min and a positive step");
    }
    const UiSliderLook& look = ui.Looks().Get<UiSliderLook>();
    const std::string_view text = ui.Text(label);
    const float textSize = options.textSize > 0.0f ? options.textSize : look.metrics.textSize;
    UiElement row = OpenUiControlRow(ui, id, text, textSize, options, look.metrics);

    const float before = value;
    if (const UiStep key = row.Step(); key.x != 0) {
        value = Snapped(value + static_cast<float>(key.x) * stride, min, max, stride);
    }
    Drag& drag = row.Remember<Drag>();
    const std::optional<Float2> pointer = ui.Pointer();
    if (row.Held() && options.enabled && pointer) {
        const FloatRect rect = row.LastRect();
        const FloatRect track = UiSliderTrack(rect, look);
        if (!drag.held) drag.onTrack = UiControlArea(rect, look.metrics).Contains(*pointer);
        drag.held = true;
        if (drag.onTrack) {
            const float share = std::clamp((pointer->x - track.x) / track.width, 0.0f, 1.0f);
            value = Snapped(min + share * (max - min), min, max, stride);
        }
    } else {
        drag.held = false;
    }
    const bool changed = value != before;
    const ButtonAnimState animation = AnimateUiControlRow(ui, row, look.metrics, false);
    const float fraction = (std::clamp(value, min, max) - min) / (max - min);
    const std::string_view number = NumberText(ui, value, decimals);
    row.Draw([look = &look, text, textSize, animation, fraction, number, accent = options.accent](
                 Painter& paint, const UiDrawState& state) {
        look->draw(paint, {.control = {.state = state,
                                       .label = text,
                                       .textSize = textSize,
                                       .animation = animation,
                                       .accent = accent,
                                       .area = UiControlArea(state.rect, look->metrics)},
                           .fraction = fraction,
                           .valueText = number,
                           .track = UiSliderTrack(state.rect, *look)});
    });
    if (!options.tooltip.empty()) UiTooltip(ui, row, options.tooltip);
    return changed;
}
} // namespace

bool UiSlider(Ui& ui, UiId id, std::string_view label, float& value, float min, float max, float step,
              const UiControlOptions& options) {
    const float stride = step > 0.0f ? step : (max - min) * DefaultStepShare;
    return RunSlider(ui, id, label, value, min, max, stride, stride < 1.0f ? 2 : 0, options);
}

bool UiSlider(Ui& ui, UiId id, std::string_view label, int& value, int min, int max, int step,
              const UiControlOptions& options) {
    float shown = static_cast<float>(value);
    const float range = static_cast<float>(max - min);
    const float stride = step > 0 ? static_cast<float>(step) : std::max(std::round(range * DefaultStepShare), 1.0f);
    const bool changed =
        RunSlider(ui, id, label, shown, static_cast<float>(min), static_cast<float>(max), stride, 0, options);
    value = static_cast<int>(std::lround(shown));
    return changed;
}
