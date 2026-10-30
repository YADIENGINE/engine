#include "Graphics/Render/Internals/Canvas.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Render/Internals/RenderTargetStack.h"

Canvas::Canvas(int width, int height, TextureFilter filter)
    : _width(width)
    , _height(height)
    , _filter(filter)
    , _target(LoadRenderTexture(width, height)) {
    SetTextureFilter(_target.texture, filter);
}

Canvas::~Canvas() { UnloadRenderTexture(_target); }

void Canvas::Fit(int windowWidth, int windowHeight) {
    if (windowWidth <= 0 || windowHeight <= 0) return; // Simge durumundaki pencere
    const Rectangle dest = Destination(windowWidth, windowHeight);
    // Küçültmede mantıksal boyutta çizilip örneklenir: ince çizgiler kaybolmaz, kenarlar yumuşar.
    const bool upscaled = dest.width > static_cast<float>(_width);
    int width = upscaled ? static_cast<int>(dest.width) : _width;
    int height = upscaled ? static_cast<int>(dest.height) : _height;
    // Çözünürlük sınırı: doku sınırda açılır, sunumda pencereye büyütülür (yazılar biraz yumuşar, çizim ucuzlar).
    if (_maxHeight > 0 && height > std::max(_maxHeight, _height)) {
        height = std::max(_maxHeight, _height);
        width = height * _width / _height;
    }
    if (width == _target.texture.width && height == _target.texture.height) return;
    UnloadRenderTexture(_target);
    _target = LoadRenderTexture(width, height);
    SetTextureFilter(_target.texture, _filter);
}

void Canvas::SetMaxHeight(int height) { _maxHeight = std::max(0, height); }

void Canvas::Begin() const { PushRenderTarget(_target, _width, _height); }

void Canvas::End() const { PopRenderTarget(); }

Rectangle Canvas::Destination(int windowWidth, int windowHeight) const {
    const auto canvasWidth = static_cast<float>(_width);
    const auto canvasHeight = static_cast<float>(_height);
    const float scale =
        std::min(static_cast<float>(windowWidth) / canvasWidth, static_cast<float>(windowHeight) / canvasHeight);

    // Boyut ve köşe tam piksel: büyütülmüş doku pencereye birebir kopyalanır.
    const float width = std::round(canvasWidth * scale);
    const float height = std::round(canvasHeight * scale);
    return {std::floor((static_cast<float>(windowWidth) - width) / 2.0f),
            std::floor((static_cast<float>(windowHeight) - height) / 2.0f), width, height};
}

std::optional<Vec2> Canvas::ToCanvas(Vec2 windowPoint, int windowWidth, int windowHeight) const {
    if (windowWidth <= 0 || windowHeight <= 0) return std::nullopt;
    const Rectangle dest = Destination(windowWidth, windowHeight);
    if (static_cast<float>(windowPoint.x) < dest.x || static_cast<float>(windowPoint.x) >= dest.x + dest.width ||
        static_cast<float>(windowPoint.y) < dest.y || static_cast<float>(windowPoint.y) >= dest.y + dest.height) {
        return std::nullopt;
    }
    return {Vec2{
        static_cast<int>((static_cast<float>(windowPoint.x) - dest.x) * static_cast<float>(_width) / dest.width),
        static_cast<int>((static_cast<float>(windowPoint.y) - dest.y) * static_cast<float>(_height) / dest.height)}};
}

void Canvas::Present(int windowWidth, int windowHeight) const {
    const Rectangle dest = Destination(windowWidth, windowHeight);
    const auto textureWidth = static_cast<float>(_target.texture.width);
    const auto textureHeight = static_cast<float>(_target.texture.height);

    // RenderTexture OpenGL'de ters durur, kaynak yüksekliği negatif verilerek çevrilir.
    const Rectangle source{0.0f, 0.0f, textureWidth, -textureHeight};

    DrawTexturePro(_target.texture, source, dest, {0.0f, 0.0f}, 0.0f, WHITE);
}
