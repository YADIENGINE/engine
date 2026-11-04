#include "Ui/Widgets/UiToggle.h"
#include "Animation/Easing.h"
#include "Ui/Looks/UiToggleLook.h"
#include "Ui/Widgets/Internals/UiControlRow.h"
#include "Ui/Widgets/UiTooltip.h"

namespace {
constexpr float KnobSpeed = 24.0f;
struct KnobPosition {
    float on = 0.0f;
};
} // namespace

bool UiToggle(Ui& ui, UiId id, std::string_view label, bool& value, const UiControlOptions& options) {
    const UiToggleLook& look = ui.Looks().Get<UiToggleLook>();
    const std::string_view text = ui.Text(label);
    const float textSize = options.textSize > 0.0f ? options.textSize : look.metrics.textSize;
    UiElement row = OpenUiControlRow(ui, id, text, textSize, options, look.metrics);

    const bool before = value;
    int user = 0;
    if (const UiAction action = row.Activated()) {
        value = !value;
        user = action.user;
    }
    if (const UiStep step = row.Step(); step.x != 0) {
        value = step.x > 0;
        user = step.user;
    }
    if (value != before) row.ReportChange(user, options.sound);
    const ButtonAnimState animation = AnimateUiControlRow(ui, row, look.metrics, value != before);

    KnobPosition& knob = row.Remember<KnobPosition>(KnobPosition{value ? 1.0f : 0.0f});
    const float target = value ? 1.0f : 0.0f;
    knob.on = target + (knob.on - target) * DecayFactor(KnobSpeed, ui.Delta());

    row.Draw([look = &look, text, textSize, animation, on = knob.on, accent = options.accent](
                 Painter& paint, const UiDrawState& state) {
        look->draw(paint, {.control = {.state = state,
                                       .label = text,
                                       .textSize = textSize,
                                       .animation = animation,
                                       .accent = accent,
                                       .area = UiControlArea(state.rect, look->metrics)},
                           .on = on});
    });
    if (!options.tooltip.empty()) UiTooltip(ui, row, options.tooltip);
    return value != before;
}
