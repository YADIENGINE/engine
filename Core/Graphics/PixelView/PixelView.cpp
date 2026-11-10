#include "Graphics/PixelView/PixelView.h"
#include <algorithm>
#include <cmath>
#include "Utilities/Geometry/Affine2.h"

namespace {
constexpr int MostDevicePixels = 1024; // Per art pixel: a zoom beyond it is not a pixel view any more
constexpr double Rounding = 1e-6;      // Device pixels a floor may miss by float error

double Positive(float value, double fallback) {
    return std::isfinite(value) && value > 0.0f ? static_cast<double>(value) : fallback;
}
} // namespace

PixelView::PixelView()
    : _image(Float2{1.0f, 1.0f}, {.emission = true, .pixels = true}) {}

PixelViewLayout PixelView::Plan(const CameraView& view, const FloatRect& area, Float2 devicePerUnit, float artPixel) {
    const double art = Positive(artPixel, 1.0);
    const double deviceX = Positive(devicePerUnit.x, 1.0);
    const double deviceY = Positive(devicePerUnit.y, 1.0);
    // Inside the image one scale for both axes, so an art pixel is square there whatever the canvas does.
    const double device = std::min(deviceX, deviceY);
    const double zoom = Positive(view.zoom, 1.0);
    const double wanted = std::round(device * art * zoom);
    const int scale = static_cast<int>(std::clamp(wanted, 1.0, static_cast<double>(MostDevicePixels)));
    const double shownZoom = scale / (art * device);

    // The camera's target on the art grid; the rest moves the image instead (in canvas units, as the view turns it).
    const double targetX = std::round(static_cast<double>(view.target.x) / art) * art;
    const double targetY = std::round(static_cast<double>(view.target.y) / art) * art;
    const Float2 rest =
        Affine2::Rotate(view.rotation)
            .Apply({static_cast<float>(view.target.x - targetX), static_cast<float>(view.target.y - targetY)});
    const double offsetX = static_cast<double>(view.offset.x) - rest.x * shownZoom;
    const double offsetY = static_cast<double>(view.offset.y) - rest.y * shownZoom;

    // Device pixels from the area's corner. The image starts within one art pixel before the viewport, on a whole
    // device pixel, and the grid point 'target' shows at lands on a whole pixel of the image.
    const FloatRect viewport = view.viewport.Empty() ? area : view.viewport;
    const auto axis = [&](double offset, float viewportStart, float viewportSize, float areaStart, double actual,
                          int& corner, int& gridPixel, int& size) {
        const double wantedAt = (offset - areaStart) * device; // Where the grid point should land
        const double start = (static_cast<double>(viewportStart) - areaStart) * actual;
        gridPixel = static_cast<int>(std::ceil((wantedAt - start) / scale - Rounding));
        corner = static_cast<int>(std::floor(wantedAt - static_cast<double>(gridPixel) * scale + Rounding));
        size = static_cast<int>(std::ceil(static_cast<double>(viewportSize) * actual / scale - Rounding)) + 2;
    };
    int cornerX = 0;
    int cornerY = 0;
    int gridX = 0;
    int gridY = 0;
    PixelViewLayout layout;
    layout.scale = scale;
    axis(offsetX, viewport.x, viewport.width, area.x, deviceX, cornerX, gridX, layout.pixels.x);
    axis(offsetY, viewport.y, viewport.height, area.y, deviceY, cornerY, gridY, layout.pixels.y);
    layout.pixels = {std::max(layout.pixels.x, 1), std::max(layout.pixels.y, 1)};

    const double originX = area.x + cornerX / deviceX;
    const double originY = area.y + cornerY / deviceY;
    const double width = static_cast<double>(layout.pixels.x) * scale;
    const double height = static_cast<double>(layout.pixels.y) * scale;
    layout.area = {static_cast<float>(originX), static_cast<float>(originY), static_cast<float>(width / device),
                   static_cast<float>(height / device)};
    layout.dest = {static_cast<float>(originX), static_cast<float>(originY), static_cast<float>(width / deviceX),
                   static_cast<float>(height / deviceY)};
    layout.stretch = {static_cast<float>(deviceX / device), static_cast<float>(deviceY / device)};
    layout.shown = view;
    layout.shown.target = {static_cast<float>(targetX), static_cast<float>(targetY)};
    layout.shown.zoom = static_cast<float>(shownZoom);
    layout.shown.offset = {static_cast<float>(originX + static_cast<double>(gridX) * scale / device),
                           static_cast<float>(originY + static_cast<double>(gridY) * scale / device)};
    return layout;
}

PaintScope PixelView::Begin(Painter& paint, const CameraView& view, std::optional<Color4> clear) {
    if (paint.CurrentTransform() != Affine2::Identity())
        paint.Reject("PixelView::Begin must be called outside paint.Camera and paint.Transform");
    const FloatRect bounds = paint.Bounds();
    const Float2 pixels = paint.TargetPixels();
    _layout = Plan(view, bounds, {pixels.x / bounds.width, pixels.y / bounds.height}, paint.ArtPixel());
    _image.Resize(ToFloat2(_layout.pixels));
    return paint.Target(_image, _layout.area, clear);
}

void PixelView::Present(Painter& paint) const {
    std::optional<PaintScope> clip;
    if (!_layout.shown.viewport.Empty()) clip.emplace(paint.Clip(_layout.shown.viewport));
    paint.Image(_image, _layout.dest);
}
