#pragma once
#include <raylib.h>
#include <functional>
#include <optional>
#include "Graphics/Render/Internals/Canvas.h"
#include "Graphics/Render/Internals/Window.h"
#include "Graphics/Render/Internals/RenderExecutionContext.h"

// Render altyapısı: pencere ve canvas'ın sahibidir, frame'leri yönetir.
// Neyin çizileceğini bilmez; her nesne kendi View'ı ile Frame içinde çizer.
class Renderer {
public:
    // RAII frame: kurulunca canvas'a çizim başlar, yok edilince canvas pencereye basılır.
    class Frame {
    public:
        ~Frame();
        // Explicit completion runs user callbacks outside the noexcept cleanup path.
        void Complete(const std::function<void(const Texture2D&)>& canvasReady = {},
                      const std::function<void()>& windowDraw = {});

    private:
        friend class Renderer;
        Frame(const Window& window, Canvas& canvas, Color background, Color letterbox,
              const RenderExecutionOptions& options, FrameObserver* observer);

        const Window& _window;
        const Canvas& _canvas;
        Color _letterbox;
        RenderExecutionContext _execution;
        FrameObserver* _observer;
        bool _canvasOpen = true;
        bool _windowOpen = false;
        bool _completed = false;

    public:
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        Frame(Frame&&) = delete;
        Frame& operator=(Frame&&) = delete;
    };

    // 'background': her frame'in canvas'ı bu renkle temizlenir.
    Renderer(const WindowSettings& window, int canvasWidth, int canvasHeight, Color background,
             TextureFilter canvasFilter = TEXTURE_FILTER_BILINEAR, Color letterbox = BLACK);

    [[nodiscard]] bool ShouldClose() const;
    [[nodiscard]] Frame BeginFrame() const;
    void SetObserver(FrameObserver* observer) { _observer = observer; }
    void SetExecutionOptions(const RenderExecutionOptions& options) { _execution = options; }
    [[nodiscard]] std::optional<Vec2> ToCanvas(Vec2 windowPoint) const;
    void RevealWindow() { _window.Show(); }
    void SetTargetFps(int fps) { _window.SetTargetFps(fps); }
    void SetBorderlessFullscreen(bool enabled) { _window.SetBorderlessFullscreen(enabled); }
    void SetVSync(bool enabled) { _window.SetVSync(enabled); }
    // Canvas dokusunun yükseklik sınırı (0: pencere çözünürlüğü); bkz. Canvas::SetMaxHeight.
    void SetMaxRenderHeight(int height) { _canvas.SetMaxHeight(height); }
    void SetWindowSize(Vec2 size) { _window.Resize(size); }
    [[nodiscard]] Vec2 MonitorSize() const { return _window.MonitorSize(); }

private:
    // Üye sırası önemli: Window (GL context) ilk kurulur, en son yok edilir.
    Window _window;
    mutable Canvas _canvas; // Her frame başında pencere boyutuna göre yeniden açılabilir (Canvas::Fit)
    Color _background;
    Color _letterbox;
    RenderExecutionOptions _execution;
    FrameObserver* _observer = nullptr;

public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
};
