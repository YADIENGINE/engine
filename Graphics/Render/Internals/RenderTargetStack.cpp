#include "Graphics/Render/Internals/RenderTargetStack.h"
#include <rlgl.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>
#include <vector>

namespace {
struct Target {
    RenderTexture2D texture;
    int width;
    int height;
};

std::vector<Target>& Targets() {
    static std::vector<Target> targets;
    return targets;
}

void Bind(const Target& target) {
    // BeginTextureMode projeksiyonu doku boyutuna kurar; çizim mantıksal boyutta kalsın diye yeniden kurulur.
    BeginTextureMode(target.texture);
    if (target.width == target.texture.texture.width && target.height == target.texture.texture.height) return;
    rlMatrixMode(RL_PROJECTION);
    rlLoadIdentity();
    rlOrtho(0.0, target.width, target.height, 0.0, 0.0, 1.0);
    rlMatrixMode(RL_MODELVIEW);
}

Vector2 TargetScale() {
    const std::vector<Target>& targets = Targets();
    if (targets.empty()) return {1.0f, 1.0f};
    const Target& top = targets.back();
    return {static_cast<float>(top.texture.texture.width) / static_cast<float>(top.width),
            static_cast<float>(top.texture.texture.height) / static_cast<float>(top.height)};
}

// Çizim koordinatından hedef pikseline eksen hizalı dönüşüm: piksel = koordinat * scale + offset.
struct AxisTransform {
    Vector2 scale;
    Vector2 offset;
};

std::optional<AxisTransform> PixelTransform() {
    constexpr float Epsilon = 1e-4f;
    // Köşe önce rlgl dönüşümünden (rlPushMatrix bloğu; dışında birim), sonra modelview'dan (kamera) geçer.
    const Matrix local = rlGetMatrixTransform();
    const Matrix view = rlGetMatrixModelview();
    if (std::abs(local.m1) > Epsilon || std::abs(local.m4) > Epsilon || std::abs(view.m1) > Epsilon ||
        std::abs(view.m4) > Epsilon) {
        return std::nullopt;
    }
    const Vector2 target = TargetScale();
    const AxisTransform transform{
        {local.m0 * view.m0 * target.x, local.m5 * view.m5 * target.y},
        {(local.m12 * view.m0 + view.m12) * target.x, (local.m13 * view.m5 + view.m13) * target.y}};
    if (transform.scale.x <= Epsilon || transform.scale.y <= Epsilon) return std::nullopt;
    return transform;
}
} // namespace

void PushRenderTarget(const RenderTexture2D& target, int width, int height) {
    Targets().push_back({target, width, height});
    Bind(Targets().back());
}

void PopRenderTarget() {
    std::vector<Target>& targets = Targets();
    if (targets.empty()) return;
    targets.pop_back();
    if (targets.empty())
        EndTextureMode();
    else
        Bind(targets.back());
}

float PixelScale() {
    // Yazı yüksekliği ölçeği: dikey eksenin boyu (döndürülmüş dönüşümde de geçerli).
    const Matrix local = rlGetMatrixTransform();
    const Matrix view = rlGetMatrixModelview();
    return std::hypot(local.m4, local.m5) * std::hypot(view.m4, view.m5) * TargetScale().y;
}

Vector2 AlignToPixel(Vector2 point) {
    const std::optional<AxisTransform> transform = PixelTransform();
    if (!transform) return point;
    const auto align = [](float value, float scale, float offset) {
        return (std::round(value * scale + offset) - offset) / scale;
    };
    return {align(point.x, transform->scale.x, transform->offset.x),
            align(point.y, transform->scale.y, transform->offset.y)};
}

Rectangle AlignToPixels(Rectangle rect) {
    const std::optional<AxisTransform> transform = PixelTransform();
    if (!transform || rect.width <= 0.0f || rect.height <= 0.0f) return rect;
    // [from, to) aralığını tam piksellere yuvarlar; en az bir piksel kalır.
    const auto align = [](float from, float to, float scale, float offset) {
        const float first = std::round(from * scale + offset);
        const float last = std::max(first + 1.0f, std::round(to * scale + offset));
        return std::pair{(first - offset) / scale, (last - first) / scale};
    };
    const auto [x, width] = align(rect.x, rect.x + rect.width, transform->scale.x, transform->offset.x);
    const auto [y, height] = align(rect.y, rect.y + rect.height, transform->scale.y, transform->offset.y);
    return {x, y, width, height};
}

void BeginTargetScissor(Rectangle rect) {
    const Vector2 scale = TargetScale();
    const int left = static_cast<int>(std::floor(rect.x * scale.x));
    const int top = static_cast<int>(std::floor(rect.y * scale.y));
    const int right = static_cast<int>(std::ceil((rect.x + rect.width) * scale.x));
    const int bottom = static_cast<int>(std::ceil((rect.y + rect.height) * scale.y));
    BeginScissorMode(left, top, std::max(0, right - left), std::max(0, bottom - top));
}
