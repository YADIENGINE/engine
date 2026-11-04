#include "Ui/Looks/UiLayerLook.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Style/Theme.h"

void DrawPlainLayerBackdrop(Painter& paint, const UiLayerView& view) {
    paint.Rect(view.state.rect, Fill(CurrentTheme().palette.overlay));
}
