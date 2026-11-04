#include "Ui/Widgets/UiTooltip.h"
#include <algorithm>
#include "Ui/Looks/UiTooltipLook.h"
#include "Ui/Widgets/UiLabel.h"

namespace {
struct TooltipMemory {
    float seconds = 0.0f;
    Float2 size{}; // Known from its second frame on
};
} // namespace

void UiTooltip(Ui& ui, UiElement& anchor, std::string_view text, const UiTooltipOptions& options) {
    const UiTooltipLook& look = ui.Looks().Get<UiTooltipLook>();
    TooltipMemory& memory = anchor.Remember<TooltipMemory>();
    const bool resting = anchor.Hovered() || (options.onFocus && anchor.Focused());
    memory.seconds = resting ? memory.seconds + ui.Delta() : 0.0f;
    if (memory.seconds < look.delay || text.empty()) return;

    // Below the element, or above it when there is no room; pushed back inside the canvas.
    const FloatRect target = anchor.LastRect();
    const FloatRect& canvas = ui.Canvas();
    const UiId id = UiId("UiTooltip").Under(anchor.Id());
    const UiLayerScope layer = ui.Layer(id, true, {.seconds = 0.0f, .modal = false});
    const Float2 size = memory.size;
    float y = target.Bottom() + look.distance;
    if (y + size.y > canvas.Bottom()) y = target.y - look.distance - size.y;
    const float maxX = std::max(canvas.x, canvas.Right() - size.x);
    const float x = std::clamp(target.Center().x - size.x * 0.5f, canvas.x, maxX);
    UiElement tip = ui.Element("tip", {.width = UiFit(0.0f, look.maxWidth),
                                       .padding = look.padding,
                                       .anchor = UiAnchor::TopLeft,
                                       .offset = {x - canvas.x, y - canvas.y}});
    memory.size = tip.Size();
    // Unseen until its size is known, so that it appears in the right place.
    const float fade = look.fadeSeconds > 0.0f ? (memory.seconds - look.delay) / look.fadeSeconds : 1.0f;
    tip.SetVisual({.opacity = size.x > 0.0f ? std::clamp(fade, 0.0f, 1.0f) : 0.0f});
    tip.Draw([look = &look](Painter& paint, const UiDrawState& state) { look->draw(paint, state); });
    UiLabel(ui, text, {.size = look.textSize, .wrap = true});
}
