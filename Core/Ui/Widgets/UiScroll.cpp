#include "Ui/Widgets/UiScroll.h"
#include <algorithm>
#include <cmath>
#include "Animation/Easing.h"
#include "Ui/Looks/UiScrollLook.h"

namespace {
constexpr float SnapDistance = 0.25f;

struct ScrollMemory {
    float offset = 0.0f;
    float target = 0.0f;
    float contentHeight = 0.0f;
    float grab = 0.0f; // Where on the thumb a drag holds it
    bool dragging = false;
};

[[nodiscard]] UiElement OpenViewport(Ui& ui, UiId id, const UiScrollOptions& options) {
    const UiScrollLook& look = ui.Looks().Get<UiScrollLook>();
    // The bar's room is always kept, so that content does not reflow when it starts to overflow.
    UiBox box{.width = options.width, .height = options.height};
    box.padding.right = look.barWidth + look.barGap;
    return ui.Element(id, box, {.clip = true});
}

// Where the thumb is along a track of 'height', from the top of the track.
[[nodiscard]] FloatRect ThumbIn(const FloatRect& track, float view, float content, float offset, float minThumb) {
    const float length = std::clamp(track.height * view / std::max(content, 1.0f), minThumb, track.height);
    const float room = track.height - length;
    const float range = std::max(content - view, 1.0f);
    return {track.x, track.y + room * std::clamp(offset / range, 0.0f, 1.0f), track.width, length};
}

// The wheel, a cursor moved by keys, and a drag on the bar decide where it is going; then it goes there.
void Advance(Ui& ui, UiElement& viewport, ScrollMemory& memory, const UiScrollLook& look) {
    const Float2 size = viewport.Size();
    const float range = std::max(memory.contentHeight - size.y, 0.0f);
    if (viewport.PointerInside()) memory.target -= ui.Wheel() * look.wheelStep;
    if (ui.CursorMovedByKeys()) {
        if (const std::optional<FloatRect> focus = viewport.FocusInside()) {
            const FloatRect view = viewport.LastRect();
            if (focus->y < view.y + look.margin) {
                memory.target = memory.offset - (view.y + look.margin - focus->y);
            } else if (focus->Bottom() > view.Bottom() - look.margin) {
                memory.target = memory.offset + (focus->Bottom() - view.Bottom() + look.margin);
            }
        }
    }
    memory.target = std::clamp(memory.target, 0.0f, range);
    memory.offset = memory.target + (memory.offset - memory.target) * DecayFactor(look.speed, ui.Delta());
    if (std::abs(memory.offset - memory.target) < SnapDistance) memory.offset = memory.target;
    memory.offset = std::clamp(memory.offset, 0.0f, range);
}

void DescribeBar(Ui& ui, UiElement& viewport, ScrollMemory& memory, const UiScrollLook& look) {
    const Float2 size = viewport.Size();
    if (memory.contentHeight <= size.y || size.y <= 0.0f) {
        memory.dragging = false;
        return;
    }
    UiElement bar = ui.Element("UiScrollBar",
                               {.width = UiFixed(look.barWidth),
                                .height = UiFill(),
                                .anchor = UiAnchor::TopRight,
                                .offset = {look.barWidth + look.barGap, 0.0f}},
                               {.interact = UiInteract::Pointer, .sound = UiSound::None});
    const FloatRect track = bar.LastRect();
    const float height = memory.contentHeight;
    const float range = height - size.y;
    if (bar.Held() && !track.Empty()) {
        if (const std::optional<Float2> pointer = ui.Pointer()) {
            const FloatRect thumb = ThumbIn(track, size.y, height, memory.offset, look.minThumb);
            if (!memory.dragging) {
                // A press on the thumb keeps hold where it is; a press beside it brings the thumb's middle there.
                memory.grab = thumb.Contains(*pointer) ? pointer->y - thumb.y : thumb.height * 0.5f;
                memory.dragging = true;
            }
            const float room = std::max(track.height - thumb.height, 1.0f);
            const float share = std::clamp((pointer->y - memory.grab - track.y) / room, 0.0f, 1.0f);
            memory.target = share * range;
            memory.offset = memory.target;
        }
    } else {
        memory.dragging = false;
    }
    bar.Draw([look = &look, view = size.y, height, offset = memory.offset, dragging = memory.dragging](
                 Painter& paint, const UiDrawState& state) {
        look->draw(paint, {.state = state,
                           .thumb = ThumbIn(state.rect, view, height, offset, look->minThumb),
                           .dragging = dragging});
    });
}

[[nodiscard]] UiElement OpenContent(Ui& ui, UiElement& viewport, UiBox content, float& offset) {
    const UiScrollLook& look = ui.Looks().Get<UiScrollLook>();
    ScrollMemory& memory = viewport.Remember<ScrollMemory>();
    Advance(ui, viewport, memory, look);
    DescribeBar(ui, viewport, memory, look);
    if (content.width.kind == UiSizeKind::Fit) content.width = UiFill();
    UiElement element = ui.Element("UiScrollContent", content);
    memory.contentHeight = element.Size().y;
    element.SetVisual({.offset = {0.0f, -memory.offset}});
    offset = memory.offset;
    return element;
}
} // namespace

UiScroll::UiScroll(Ui& ui, UiId id, UiBox content, const UiScrollOptions& options)
    : _viewport(OpenViewport(ui, id, options))
    , _content(OpenContent(ui, _viewport, content, _offset)) {}

void UiScroll::Close() {
    _content.Close();
    _viewport.Close();
}
