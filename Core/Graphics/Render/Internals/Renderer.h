#pragma once
#include <raylib.h>
#include <functional>
#include <memory>
#include <optional>
#include "Graphics/Gpu/Internals/GpuTypes.h"
#include "Graphics/Render/Internals/RenderPipeline.h"
#include "Graphics/Render/Internals/Window.h"
#include "Utilities/FrameObserver.h"

class GlDevice;
class Painter;

// The window plus everything that draws into it: the OpenGL device, the frame pipeline (canvas, post-processing,
// presentation).
class Renderer {
public:
    // RAII frame: opens the canvas Painter; Complete (or destruction) executes, post-processes and presents it.
    class Frame {
    public:
        ~Frame();
        [[nodiscard]] Painter& Canvas() { return _canvas; }
        // 'canvasReady' borrows the finished canvas image (for capture) during the call; 'windowDraw' draws over the
        // whole window in device pixels after the canvas was presented; 'toolOverlay' renders natively (ImGui) on
        // top of both.
        void Complete(const std::function<void(const Texture2D&)>& canvasReady = {},
                      const std::function<void(Painter&)>& windowDraw = {},
                      const std::function<void()>& toolOverlay = {});

    private:
        friend class Renderer;
        Frame(Renderer& renderer, Painter& canvas);
        Renderer& _renderer;
        Painter& _canvas;
        bool _completed = false;

    public:
        Frame(const Frame&) = delete;
        Frame& operator=(const Frame&) = delete;
        Frame(Frame&&) = delete;
        Frame& operator=(Frame&&) = delete;
    };

    Renderer(const WindowSettings& window, const RenderPipelineSettings& settings);
    ~Renderer();

    [[nodiscard]] bool ShouldClose() const;
    [[nodiscard]] Frame BeginFrame(double time, float delta);
    void SetObserver(FrameObserver* observer) { _observer = observer; }
    void SetQuality(const RenderQuality& quality) { _pipeline->SetQuality(quality); }
    void SetPostDefaults(const PostSettings& post) { _pipeline->SetPostDefaults(post); }
    void SetFont(const FontFace* font) { _pipeline->SetFont(font); }
    [[nodiscard]] const RenderStats& LastFrame() const { return _pipeline->LastFrame(); }
    [[nodiscard]] std::optional<Vec2> ToCanvas(Vec2 windowPoint) const;
    void RevealWindow() { _window.Show(); }
    void SetTargetFps(int fps) { _window.SetTargetFps(fps); }
    void SetBorderlessFullscreen(bool enabled) { _window.SetBorderlessFullscreen(enabled); }
    void SetVSync(bool enabled) { _window.SetVSync(enabled); }
    // Canvas texture height limit (0: the window's); see RenderPipeline::SetMaxRenderHeight.
    void SetMaxRenderHeight(int height) { _pipeline->SetMaxRenderHeight(height); }
    void SetWindowSize(Vec2 size) { _window.Resize(size); }
    [[nodiscard]] Vec2 MonitorSize() const { return _window.MonitorSize(); }

private:
    // Member order matters: the window (GL context) is created first and destroyed last; the pipeline's GPU objects
    // go before the device.
    Window _window;
    std::unique_ptr<GlDevice> _device;
    std::unique_ptr<RenderPipeline> _pipeline;
    FrameObserver* _observer = nullptr;

public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
    Renderer(Renderer&&) = delete;
    Renderer& operator=(Renderer&&) = delete;
};
