#include "Editor/Internals/Session/ViewportMark.h"
#include <algorithm>
#include <cmath>
#include <format>
#include "Editor/Internals/Session/AgentContext.h"

namespace {
std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {reinterpret_cast<const char*>(text.data()), text.size()};
}
Float2 ToCanvas(const SceneViewport& viewport, Float2 point) {
    return {(point.x - viewport.position.x) / viewport.size.x * static_cast<float>(viewport.canvas.x),
            (point.y - viewport.position.y) / viewport.size.y * static_cast<float>(viewport.canvas.y)};
}
} // namespace

std::optional<FloatRect> MarkRegion(const SceneViewport& viewport, Float2 start, Float2 end) {
    if (viewport.size.x <= 0.0f || viewport.size.y <= 0.0f || viewport.canvas.x <= 0 || viewport.canvas.y <= 0 ||
        !MapViewportPoint(viewport, start))
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
std::optional<FloatRect> WindowMarkRegion(Float2 start, Float2 end, Float2 windowPosition, Float2 windowSize,
                                          Float2 scale) {
    if (windowSize.x <= 0.0f || windowSize.y <= 0.0f || scale.x <= 0.0f || scale.y <= 0.0f) return std::nullopt;
    const float width = std::round(windowSize.x * scale.x), height = std::round(windowSize.y * scale.y);
    const auto toPixels = [&](Float2 point) {
        return Float2{(point.x - windowPosition.x) * scale.x, (point.y - windowPosition.y) * scale.y};
    };
    const Float2 from = toPixels(start), to = toPixels(end);
    if (std::abs(end.x - start.x) < MarkClickPixels && std::abs(end.y - start.y) < MarkClickPixels) {
        const float side = std::min({MarkWindowPointSize, width, height});
        return FloatRect{std::round(std::clamp(from.x - side * 0.5f, 0.0f, width - side)),
                         std::round(std::clamp(from.y - side * 0.5f, 0.0f, height - side)), side, side};
    }
    const float left = std::round(std::clamp(std::min(from.x, to.x), 0.0f, width));
    const float right = std::round(std::clamp(std::max(from.x, to.x), 0.0f, width));
    const float top = std::round(std::clamp(std::min(from.y, to.y), 0.0f, height));
    const float bottom = std::round(std::clamp(std::max(from.y, to.y), 0.0f, height));
    if (right - left < 1.0f || bottom - top < 1.0f) return std::nullopt;
    return FloatRect{left, top, right - left, bottom - top};
}
std::string WindowMarkContext(FloatRect region, Vec2 window, const std::filesystem::path& image,
                              const std::string& scene) {
    std::string line = std::format("[editor] window region={},{} {}x{} of {}x{} image=\"{}\"", region.x, region.y,
                                   region.width, region.height, window.x, window.y, Utf8(image));
    if (!scene.empty()) line += " open-scene=" + scene;
    return line + " ";
}
std::string MarkContext(const SceneDefinition& definition, const SceneParameters& values, double seconds, bool paused,
                        FloatRect region, Vec2 canvas, const std::filesystem::path& image) {
    std::string line = std::format("[editor] scene={} t{}{:.2f}s({}) region={},{} {}x{} of {}x{} image=\"{}\"",
                                   definition.id, paused ? "=" : "~", seconds, paused ? "paused" : "playing", region.x,
                                   region.y, region.width, region.height, canvas.x, canvas.y, Utf8(image));
    if (const auto changed = ChangedParameters(definition, values); !changed.empty()) line += " params:" + changed;
    return line + " ";
}
