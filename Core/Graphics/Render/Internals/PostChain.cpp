#include "Graphics/Render/Internals/PostChain.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
constexpr int MaxBloomLevels = 6;
constexpr int MinBloomSide = 8; // A level smaller than this ends the chain
constexpr float MinSpread = 0.25f;
constexpr float MaxSpread = 4.0f;
constexpr double NoiseFramesPerSecond = 24.0; // Film's rate: faster reads as television static
constexpr double NoiseFrames = 16777216.0;    // Whole numbers a float holds exactly: 8 days of moments
// Compose.glsl's wobble waves all run at multiples of 1/16, so the rhythm wraps here without a jump.
constexpr double WobbleWrap = 2.0 * 3.14159265358979323846 * 16.0;
constexpr double TapeRollsPerSecond = 0.07; // A band crosses the image in about 14 seconds at speed 1
constexpr float TapeBandPush = 28.0f;       // Canvas units a full band pushes the image
constexpr float TapeLineHeight = 2.0f;      // Canvas units

PostUniforms Texel(int width, int height) {
    PostUniforms uniforms;
    uniforms.sourceTexel = {1.0f / static_cast<float>(width), 1.0f / static_cast<float>(height)};
    return uniforms;
}
} // namespace

PostChain::PostChain(GpuDevice& device)
    : _device(device) {}

PostChain::~PostChain() { Release(); }

bool PostChain::Blooms(const PostSettings& settings, const RenderQuality& quality) {
    return quality.bloom != BloomQuality::Off && settings.bloom.intensity > 0.0f;
}

bool PostChain::Active(const PostSettings& settings, const RenderQuality& quality) {
    const Color4 tint = settings.tint.color;
    const bool tinted = settings.tint.amount > 0.0f && (tint.r != 255 || tint.g != 255 || tint.b != 255);
    const bool moving = quality.screenMotion && (settings.grain.amount > 0.0f || settings.wobble.amount > 0.0f ||
                                                 settings.tape.band > 0.0f || settings.tape.shift > 0.0f);
    return Blooms(settings, quality) || settings.vignette > 0.0f || settings.scanlines > 0.0f ||
           settings.aberration > 0.0f || settings.fade > 0.0f || tinted || moving;
}

void PostChain::Release() {
    if (_composed.target.Valid()) _device.DestroyTarget(_composed.target);
    for (const Level& level : _bloom)
        _device.DestroyTarget(level.target);
    _composed = {};
    _bloom.clear();
}

void PostChain::Prepare(int width, int height, BloomQuality bloom) {
    if (_composed.width == width && _composed.height == height && _bloomQuality == bloom && _composed.target.Valid())
        return;
    Release();
    _bloomQuality = bloom;
    _composed = {_device.CreateTarget({width, height, GpuFormat::Rgba8, GpuFilter::Linear, false, "Composed"}), width,
                 height};
    if (bloom == BloomQuality::Off) return;
    int levelWidth = bloom == BloomQuality::Half ? width / 2 : width;
    int levelHeight = bloom == BloomQuality::Half ? height / 2 : height;
    while (static_cast<int>(_bloom.size()) < MaxBloomLevels && levelWidth >= MinBloomSide &&
           levelHeight >= MinBloomSide) {
        _bloom.push_back(
            {_device.CreateTarget({levelWidth, levelHeight, GpuFormat::Rgba16F, GpuFilter::Linear, false, "Bloom"}),
             levelWidth, levelHeight});
        levelWidth /= 2;
        levelHeight /= 2;
    }
}

void PostChain::Pass(const Level& level, std::span<const GpuTexture> sources, GpuProgram program, GpuBlend blend,
                     const PostUniforms& uniforms, bool clear) {
    GpuPassDesc pass;
    pass.target = level.target;
    pass.viewport = {0, 0, level.width, level.height};
    pass.viewSize = {static_cast<float>(level.width), static_cast<float>(level.height)};
    if (clear) pass.clear = PremultipliedColor{};
    _device.BeginPass(pass);
    _device.SetPipeline(program, blend);
    _device.SetScissor(nullptr);
    _device.BindTextures(sources);
    _device.DrawFullscreen(uniforms);
    _device.EndPass();
}

GpuTarget PostChain::Run(GpuTarget canvas, int width, int height, float canvasHeight, const PostSettings& settings,
                         const RenderQuality& quality, double time) {
    const bool blooms = Blooms(settings, quality);
    Prepare(width, height, blooms ? quality.bloom : BloomQuality::Off);
    const float spread = std::clamp(settings.bloom.radius, MinSpread, MaxSpread);
    if (blooms && !_bloom.empty()) {
        _device.PushMarker("Bloom");
        // Down: emission -> level 0 -> level 1 ...
        GpuTexture source = _device.TargetTexture(canvas, 1);
        int sourceWidth = width;
        int sourceHeight = height;
        for (const Level& level : _bloom) {
            PostUniforms uniforms = Texel(sourceWidth, sourceHeight);
            uniforms.bloomSpread = spread;
            const std::array textures{source};
            Pass(level, textures, GpuProgram::BloomDownsample, GpuBlend::Replace, uniforms, false);
            source = _device.TargetTexture(level.target, 0);
            sourceWidth = level.width;
            sourceHeight = level.height;
        }
        // Up: each level adds its blurred smaller neighbour (alpha 0 output: additive).
        for (std::size_t i = _bloom.size() - 1; i > 0; --i) {
            PostUniforms uniforms = Texel(_bloom[i].width, _bloom[i].height);
            uniforms.bloomSpread = spread;
            const std::array textures{_device.TargetTexture(_bloom[i].target, 0)};
            Pass(_bloom[i - 1], textures, GpuProgram::BloomUpsample, GpuBlend::Premultiplied, uniforms, false);
        }
        _device.PopMarker();
    }
    PostUniforms compose = Texel(width, height);
    compose.bloomIntensity = blooms && !_bloom.empty() ? settings.bloom.intensity : 0.0f;
    compose.vignette = std::clamp(settings.vignette, 0.0f, 1.0f);
    compose.scanlines = std::clamp(settings.scanlines, 0.0f, 1.0f);
    compose.scanlineCount = settings.scanlineCount > 0.0f ? settings.scanlineCount : canvasHeight * 0.5f;
    // Aberration is given in canvas units; the shader offsets in source texels.
    compose.aberration = std::max(settings.aberration, 0.0f) * static_cast<float>(height) / canvasHeight;
    compose.fade = std::clamp(settings.fade, 0.0f, 1.0f);
    const float tint = std::clamp(settings.tint.amount, 0.0f, 1.0f);
    const auto tinted = [tint](std::uint8_t channel) {
        return 1.0f + (static_cast<float>(channel) / 255.0f - 1.0f) * tint;
    };
    compose.tintRed = tinted(settings.tint.color.r);
    compose.tintGreen = tinted(settings.tint.color.g);
    compose.tintBlue = tinted(settings.tint.color.b);

    // A clock that stands still or runs back (a seek in the editor) leaves the rhythms where they are.
    const double elapsed = std::max(time - _lastTime, 0.0);
    _lastTime = time;
    _wobblePhase = std::fmod(_wobblePhase + elapsed * std::max(settings.wobble.speed, 0.0f), WobbleWrap);
    _tapeRoll = std::fmod(_tapeRoll + elapsed * TapeRollsPerSecond * std::max(settings.tape.speed, 0.0f), 1.0);
    compose.noiseFrame = static_cast<float>(std::fmod(std::floor(time * NoiseFramesPerSecond), NoiseFrames));
    compose.wobblePhase = static_cast<float>(_wobblePhase);
    compose.tapeRoll = static_cast<float>(_tapeRoll);
    // Canvas units to source texels.
    const float texels = static_cast<float>(height) / canvasHeight;
    compose.grainCell = std::max(settings.grain.size * texels, 1.0f);
    compose.tapeLine = std::max(TapeLineHeight * texels, 1.0f);
    if (quality.screenMotion) {
        compose.grain = std::clamp(settings.grain.amount, 0.0f, 1.0f);
        compose.wobble = std::max(settings.wobble.amount, 0.0f) * texels;
        compose.tapeBand = std::clamp(settings.tape.band, 0.0f, 1.0f);
        compose.tapePush = compose.tapeBand * TapeBandPush * texels;
        compose.tapeShift = std::max(settings.tape.shift, 0.0f) * texels;
    }
    const GpuTexture color = _device.TargetTexture(canvas, 0);
    const GpuTexture bloom = compose.bloomIntensity > 0.0f ? _device.TargetTexture(_bloom.front().target, 0) : color;
    const std::array textures{color, bloom};
    _device.PushMarker("Compose");
    Pass(_composed, textures, GpuProgram::Compose, GpuBlend::Replace, compose, false);
    _device.PopMarker();
    return _composed.target;
}
