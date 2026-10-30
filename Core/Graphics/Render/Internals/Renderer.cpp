#include "Graphics/Render/Internals/Renderer.h"
#include <raylib.h>

Renderer::Frame::Frame(const Window& window, Canvas& canvas, Color background, Color letterbox,
                       const RenderExecutionOptions& options, FrameObserver* observer)
    : _window(window)
    , _canvas(canvas)
    , _letterbox(letterbox)
    , _execution(options, observer)
    , _observer(observer) {
    if (_observer) _observer->BeginRegion(FrameRegion::Canvas);
    canvas.Fit(_window.Width(), _window.Height());
    _canvas.Begin();
    ClearBackground(background);
}

Renderer::Frame::~Frame() {
    if (_canvasOpen) _canvas.End();
    if (_windowOpen) EndDrawing();
    // Existing scoped-frame consumers still get the default presentation.
    if (!_completed && !_windowOpen) {
        BeginDrawing();
        ClearBackground(_letterbox);
        _canvas.Present(_window.Width(), _window.Height());
        EndDrawing();
    }
}

void Renderer::Frame::Complete(const std::function<void(const Texture2D&)>& canvasReady,
                               const std::function<void()>& windowDraw) {
    if (_completed) return;
    _canvas.End();
    if (_observer) _observer->EndRegion(FrameRegion::Canvas);
    _canvasOpen = false;
    _completed = true;
    if (_observer) _observer->BeginRegion(FrameRegion::PreviewBridge);
    if (canvasReady) canvasReady(_canvas.Texture());
    if (_observer) {
        _observer->EndRegion(FrameRegion::PreviewBridge);
        _observer->BeginRegion(FrameRegion::Present);
    }
    BeginDrawing();
    _windowOpen = true;
    ClearBackground(_letterbox);
    _canvas.Present(_window.Width(), _window.Height());
    if (windowDraw) windowDraw();
    if (_observer) {
        _observer->EndRegion(FrameRegion::Present);
        _observer->BeginRegion(FrameRegion::EndDrawing);
    }
    EndDrawing();
    if (_observer) _observer->EndRegion(FrameRegion::EndDrawing);
    _windowOpen = false;
}

Renderer::Renderer(const WindowSettings& window, int canvasWidth, int canvasHeight, Color background,
                   TextureFilter canvasFilter, Color letterbox)
    : _window(window)
    , _canvas(canvasWidth, canvasHeight, canvasFilter)
    , _background(background)
    , _letterbox(letterbox) {}

bool Renderer::ShouldClose() const { return _window.ShouldClose(); }

Renderer::Frame Renderer::BeginFrame() const {
    return Frame(_window, _canvas, _background, _letterbox, _execution, _observer);
}

std::optional<Vec2> Renderer::ToCanvas(Vec2 windowPoint) const {
    return _canvas.ToCanvas(windowPoint, _window.Width(), _window.Height());
}
