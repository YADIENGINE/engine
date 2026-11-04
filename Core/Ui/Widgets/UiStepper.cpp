#include "Ui/Widgets/UiStepper.h"
#include <algorithm>
#include <stdexcept>
#include "Ui/Looks/UiStepperLook.h"
#include "Ui/Widgets/Internals/UiControlRow.h"
#include "Ui/Widgets/UiTooltip.h"
#include "Utilities/Math/Index.h"

namespace {
// 'canChange': a list of one choice steps nowhere, so that stays silent.
[[nodiscard]] int RunStepper(Ui& ui, UiId id, std::string_view label, std::string_view value,
                             const UiControlOptions& options, bool canChange) {
    const UiStepperLook& look = ui.Looks().Get<UiStepperLook>();
    const std::string_view text = ui.Text(label);
    const std::string_view shown = ui.Text(value);
    const float textSize = options.textSize > 0.0f ? options.textSize : look.metrics.textSize;
    UiElement row = OpenUiControlRow(ui, id, text, textSize, options, look.metrics);

    const UiStep key = row.Step();
    int stepped = key.x;
    int user = key.user;
    if (const UiAction action = row.Activated()) {
        stepped = 1;
        user = action.user;
        if (const std::optional<Float2> pointer = ui.Pointer(); action.byPointer && pointer) {
            const FloatRect area = UiControlArea(row.LastRect(), look.metrics);
            if (area.Contains(*pointer) && pointer->x < area.Center().x) stepped = -1;
        }
    }
    if (stepped != 0 && canChange) row.ReportChange(user, options.sound);
    const ButtonAnimState animation = AnimateUiControlRow(ui, row, look.metrics, stepped != 0);
    row.Draw([look = &look, text, shown, textSize, animation, accent = options.accent](Painter& paint,
                                                                                       const UiDrawState& state) {
        look->draw(paint, {.control = {.state = state,
                                       .label = text,
                                       .textSize = textSize,
                                       .animation = animation,
                                       .accent = accent,
                                       .area = UiControlArea(state.rect, look->metrics)},
                           .valueText = shown});
    });
    if (!options.tooltip.empty()) UiTooltip(ui, row, options.tooltip);
    return stepped;
}
} // namespace

int UiStepper(Ui& ui, UiId id, std::string_view label, std::string_view value, const UiControlOptions& options) {
    return RunStepper(ui, id, label, value, options, true);
}

bool UiStepper(Ui& ui, UiId id, std::string_view label, int& selected, std::span<const std::string_view> choices,
               const UiControlOptions& options) {
    if (choices.empty()) throw std::invalid_argument("UiStepper: needs at least one choice");
    const int count = static_cast<int>(choices.size());
    const int current = std::clamp(selected, 0, count - 1);
    const int stepped = RunStepper(ui, id, label, choices[static_cast<std::size_t>(current)], options, count > 1);
    selected = WrapIndex(current + stepped, count);
    return stepped != 0 && count > 1;
}
