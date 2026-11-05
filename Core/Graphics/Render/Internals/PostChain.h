#pragma once
#include <span>
#include <vector>
#include "Graphics/Gpu/Internals/GpuDevice.h"
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Post/RenderQuality.h"

// Full-screen finish of the canvas: bloom from its emission attachment (a down / up sampling chain), then one compose
// pass (colour + bloom, vignette, scanlines, chromatic aberration, and the analog finish: fade, tint, grain, wobble,
// tape) into the image that is presented.
class PostChain {
public:
    explicit PostChain(GpuDevice& device);
    ~PostChain();

    // Whether 'settings' need any work at 'quality' (otherwise the canvas is presented as it is).
    [[nodiscard]] static bool Active(const PostSettings& settings, const RenderQuality& quality);
    [[nodiscard]] static bool Blooms(const PostSettings& settings, const RenderQuality& quality);
    // Runs on 'canvas' (width x height device pixels; with an emission attachment when Blooms). 'canvasHeight' is the
    // canvas's logical height (scanline count). 'time' is the presentation clock in seconds: stopping it stops the
    // grain, the wobble and the tape. Returns the composed image (owned by the chain).
    [[nodiscard]] GpuTarget Run(GpuTarget canvas, int width, int height, float canvasHeight,
                                const PostSettings& settings, const RenderQuality& quality, double time);

private:
    struct Level {
        GpuTarget target;
        int width = 0;
        int height = 0;
    };
    void Prepare(int width, int height, BloomQuality bloom);
    void Release();
    void Pass(const Level& level, std::span<const GpuTexture> sources, GpuProgram program, GpuBlend blend,
              const PostUniforms& uniforms, bool clear);

    GpuDevice& _device;
    Level _composed;
    std::vector<Level> _bloom; // Largest first
    BloomQuality _bloomQuality = BloomQuality::Off;
    // Rhythms are added up frame by frame: multiplying the clock by a speed would jump whenever the speed changes.
    double _lastTime = 0.0;
    double _wobblePhase = 0.0;
    double _tapeRoll = 0.0;

public:
    PostChain(const PostChain&) = delete;
    PostChain& operator=(const PostChain&) = delete;
};
