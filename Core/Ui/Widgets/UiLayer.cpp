#include "Ui/Widgets/UiLayer.h"
#include "Ui/Looks/UiLayerLook.h"

UiLayerScope UiLayer(Ui& ui, UiId id, bool open, UiLayerSpec spec) {
    const UiLayerLook& look = ui.Looks().Get<UiLayerLook>();
    if (!spec.seconds) spec.seconds = look.seconds;
    UiLayerScope layer = ui.Layer(id, open, spec);
    if (layer) {
        const float progress = layer.Open();
        layer.SetVisual({.opacity = progress});
        layer.Draw([look = &look, progress](Painter& paint, const UiDrawState& state) {
            look->backdrop(paint, {.state = state, .open = progress});
        });
    }
    return layer;
}
