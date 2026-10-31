#pragma once
#include <optional>
#include <string>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Paint/Internals/PaintContext.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Post/RenderQuality.h"
#include "Graphics/Render/Internals/FrameExecutor.h"
#include "Graphics/Render/Internals/PostChain.h"
#include "Graphics/Render/Internals/RenderResources.h"
#include "Graphics/Render/RenderStats.h"
#include "Graphics/Text/Internals/GlyphCache.h"
#include "Graphics/Text/Internals/TextLayoutCache.h"
#include "Utilities/Color4.h"
#include "Utilities/Vec2.h"

struct RenderPipelineSettings {
    Float2 canvasSize{1280.0f, 720.0f}; // Logical canvas (drawing coordinates)
    Color4 background{0, 0, 0, 255};    // Canvas clear colour
    Color4 letterbox{0, 0, 0, 255};     // Window area around the canvas
    bool smoothCanvas = true;           // Linear filtering when the canvas is scaled to the window
    PostSettings post;                  // The game's post-processing defaults
};

// One frame of the renderer, GPU-independent (works on any GpuDevice):
//   BeginFrame -> draw with the canvas Painter -> FinishCanvas (executes, post-processes) ->
//   BeginWindow -> draw window overlays -> FlushWindow -> (native tool overlays) -> EndFrame.
// The canvas keeps its logical size; it is rasterized at the window's pixel size (up to MaxRenderHeight), and
// presented letterboxed.
class RenderPipeline {
public:
    RenderPipeline(GpuDevice& device, const RenderPipelineSettings& settings);
    ~RenderPipeline();

    [[nodiscard]] Painter& BeginFrame(Vec2 windowSize, double time, float delta);
    // The finished canvas image (after post-processing): borrowed until EndFrame.
    [[nodiscard]] GpuTarget FinishCanvas();
    // Painter over the whole window (device pixels), after the canvas was presented into it.
    [[nodiscard]] Painter& BeginWindow();
    // Executes the window drawing recorded so far, so native tool overlays drawn next land on top of it.
    void FlushWindow();
    void EndFrame();
    [[nodiscard]] bool InFrame() const { return _phase != Phase::Idle; }

    void SetQuality(const RenderQuality& quality) { _quality = quality; }
    [[nodiscard]] const RenderQuality& Quality() const { return _quality; }
    void SetPostDefaults(const PostSettings& post) { _settings.post = post; }
    // The game's font for Painter::Font (null: none); borrowed, must outlive the pipeline's frames.
    void SetFont(const FontFace* font) { _context.font = font; }
    // Upper limit for the canvas texture's height in pixels (0: the window's); never below the logical height.
    void SetMaxRenderHeight(int height) { _maxRenderHeight = std::max(0, height); }
    // Window pixel -> canvas point; nullopt in the letterbox.
    [[nodiscard]] std::optional<Vec2> ToCanvas(Vec2 windowPoint, Vec2 windowSize) const;
    [[nodiscard]] const RenderStats& LastFrame() const { return _stats; }
    [[nodiscard]] GpuDevice& Device() const { return _device; }
    // Where the canvas lands in a window of 'windowSize' pixels (aspect kept, centred, whole pixels).
    [[nodiscard]] FloatRect Letterbox(Vec2 windowSize) const;

private:
    enum class Phase { Idle, Canvas, Window };
    void PrepareCanvas(Vec2 windowSize, bool emission);
    void BeginRecording(const PaintTarget& root, std::optional<PremultipliedColor> clear);
    // Closes the recording, executes it and counts what it rejected.
    void FinishRecording();

    GpuDevice& _device;
    RenderPipelineSettings _settings;
    RenderQuality _quality;
    int _maxRenderHeight = 0;
    RenderResources _resources;
    GlyphCache _glyphs;
    TextLayoutCache _layouts;
    TextServices _text{_glyphs, _layouts};
    DrawList _list;
    FrameExecutor _executor;
    PaintContext _context;
    Painter _painter{_context};
    PostChain _post;
    GpuTarget _canvas;
    int _canvasWidth = 0;
    int _canvasHeight = 0;
    bool _canvasEmission = false;
    bool _bloomRequested = false; // The last frame's post settings asked for bloom
    GpuTarget _presented;
    Vec2 _windowSize;
    Phase _phase = Phase::Idle;
    RenderStats _stats;
    std::string _reportedError; // Last rejection written to stderr (each distinct one once in a row)

public:
    RenderPipeline(const RenderPipeline&) = delete;
    RenderPipeline& operator=(const RenderPipeline&) = delete;
};
