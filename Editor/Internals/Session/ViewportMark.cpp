#include "Editor/Internals/Session/ViewportMark.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "Editor/Internals/Protocol/EditorControlProtocol.h"

namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
Float2 ToCanvas(const PreviewViewport& viewport, Float2 point) {
    return {(point.x - viewport.position.x) / viewport.size.x * static_cast<float>(viewport.canvas.x),
            (point.y - viewport.position.y) / viewport.size.y * static_cast<float>(viewport.canvas.y)};
}
} // namespace

std::optional<FloatRect> MarkRegion(const PreviewViewport& viewport, Float2 start, Float2 end) {
    if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f || viewport.canvas.x <= 0 || viewport.canvas.y <= 0 ||
        !MapPreviewPoint(viewport, start))
        return std::nullopt;
    const auto width = static_cast<float>(viewport.canvas.x), height = static_cast<float>(viewport.canvas.y);
    const auto clampX = [width](float x) { return std::clamp(x, 0.0f, width); };
    const auto clampY = [height](float y) { return std::clamp(y, 0.0f, height); };
    const Float2 from = ToCanvas(viewport, start);
    if (std::abs(end.x - start.x) < MarkClickPixels && std::abs(end.y - start.y) < MarkClickPixels) {
        const float side = std::min({MarkPointSize, width, height});
        const float left = std::clamp(from.x - side * 0.5f, 0.0f, width - side);
        const float top = std::clamp(from.y - side * 0.5f, 0.0f, height - side);
        return FloatRect{std::round(left), std::round(top), side, side};
    }
    const Float2 to = ToCanvas(viewport, end);
    const float left = std::round(clampX(std::min(from.x, to.x))), right = std::round(clampX(std::max(from.x, to.x)));
    const float top = std::round(clampY(std::min(from.y, to.y))), bottom = std::round(clampY(std::max(from.y, to.y)));
    if (right - left < 1.0f || bottom - top < 1.0f) return std::nullopt;
    return FloatRect{left, top, right - left, bottom - top};
}
std::string MarkContext(const PreviewDefinition& definition, const PreviewParameters& values, double seconds,
                        bool paused, FloatRect region, Vec2 canvas, const std::filesystem::path& image) {
    std::string line = std::format("[editor] preview={} t{}{:.2f}s({}) region={},{} {}x{} of {}x{} image=\"{}\"",
                                   definition.id, paused ? "=" : "~", seconds, paused ? "paused" : "playing", region.x,
                                   region.y, region.width, region.height, canvas.x, canvas.y, Utf8(image));
    std::string changed;
    for (const auto& parameter : definition.parameters) {
        const auto value = values.find(parameter.id);
        if (value == values.end() || value->second == parameter.initial) continue;
        changed += " " + parameter.id + "=" + ControlValue(value->second).dump();
    }
    if (!changed.empty()) line += " params:" + changed;
    return line + " ";
}
