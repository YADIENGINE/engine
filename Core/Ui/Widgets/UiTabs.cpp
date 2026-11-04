#include "Ui/Widgets/UiTabs.h"
#include <algorithm>
#include <stdexcept>
#include "Animation/ButtonAnim.h"
#include "Graphics/Text/FontFace.h"
#include "Ui/Looks/UiTabLook.h"
#include "Utilities/MathUtils.h"

namespace {
// A click on a later tab decides after the earlier ones were described, so tabs read the choice when they are drawn.
struct TabsMemory {
    int current = 0;
};
} // namespace

bool UiTabs(Ui& ui, UiId id, std::span<const std::string_view> labels, int& selected, const UiTabsOptions& options) {
    if (labels.empty()) throw std::invalid_argument("UiTabs: needs at least one tab");
    const UiTabLook& look = ui.Looks().Get<UiTabLook>();
    const float textSize = options.textSize > 0.0f ? options.textSize : look.textSize;
    const FontFace* font = &ui.Font();
    const int count = static_cast<int>(labels.size());
    UiElement row = ui.Element(
        id, {.width = options.fill ? UiFill() : UiFit(), .direction = UiDirection::Row, .gap = look.gap},
        {.interact = UiInteract::Focus, .capture = UiCapture::Horizontal, .sound = UiSound::None});
    const bool cursor = row.Focused();

    TabsMemory& memory = row.Remember<TabsMemory>();
    memory.current = std::clamp(selected, 0, count - 1);
    const int shown = memory.current;
    int user = 0;
    if (const UiStep step = row.Step(); step.x != 0) {
        memory.current = WrapIndex(memory.current + step.x, count);
        user = step.user;
    }
    if (const int page = options.pageKeys ? ui.PageStep() : 0; page != 0) {
        memory.current = WrapIndex(memory.current + page, count);
    }

    for (int index = 0; index < count; ++index) {
        const std::string_view text = ui.Text(labels[static_cast<std::size_t>(index)]);
        const UiBox box{.width = options.fill ? UiFill() : UiFit(look.minWidth),
                        .height = UiFit(look.minHeight),
                        .padding = look.padding};
        UiTraits traits{.interact = UiInteract::Pointer, .sound = UiSound::None};
        traits.measure = [font, text, textSize](float) { return font->Measure(text, textSize); };
        UiElement tab = ui.Element(index, box, traits);
        if (const UiAction click = tab.Activated()) {
            memory.current = index;
            user = click.user;
            row.TakeFocus(click.user);
        }
        auto& animation = tab.Remember<ButtonAnim>(look.animation);
        animation.Update(index == memory.current, ui.Delta());
        tab.Draw([look = &look, memory = &memory, index, text, textSize, cursor, state = animation.State(),
                  accent = options.accent](Painter& paint, const UiDrawState& drawn) {
            const bool chosen = memory->current == index;
            look->draw(paint, {.state = drawn,
                               .label = text,
                               .textSize = textSize,
                               .selected = chosen,
                               .cursor = chosen && cursor,
                               .animation = state,
                               .accent = accent});
        });
    }
    if (memory.current != shown) row.ReportChange(user, options.sound);
    const bool changed = memory.current != selected;
    selected = memory.current;
    return changed;
}
