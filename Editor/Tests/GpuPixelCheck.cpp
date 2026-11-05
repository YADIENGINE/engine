// Opt-in check of what the GPU really draws: a hidden window renders with the real device and reads the pixels back.
// The NullDevice tests see vertices and commands, not shaders or GL state; these cover that gap. Not a CTest: it needs
// a desktop/GPU, like EngineEditorUiSmoke.
//   EngineGpuPixelCheck
#include "external/glad.h"
#include <raylib.h>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include "Graphics/Gpu/Internals/GlDevice.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Render/Internals/RenderPipeline.h"
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Surface.h"
#include "Utilities/Math/Scalar.h"

namespace {
constexpr int WindowSize = 256;
constexpr Color4 White{255, 255, 255, 255};
constexpr Color4 Gray{128, 128, 128, 255};
constexpr Color4 Black{0, 0, 0, 255};

struct Pixel {
    int r = 0;
    int g = 0;
    int b = 0;
    int a = 0;
};

class PixelCheck {
public:
    PixelCheck()
        : _pipeline(_device, {.canvasSize = {WindowSize, WindowSize}}) {}

    // One frame: 'draw' records, the frame executes. Returns the finished canvas image (valid until the next frame).
    GpuTarget Frame(const std::function<void(Painter&)>& draw) {
        Painter& paint = _pipeline.BeginFrame({WindowSize, WindowSize}, 0.0, 0.0f);
        draw(paint);
        const GpuTarget shown = _pipeline.FinishCanvas();
        (void)_pipeline.BeginWindow();
        _pipeline.EndFrame();
        return shown;
    }

    // Logical coordinates, y down. Attachment 1 is the surface's bloom source.
    [[nodiscard]] Pixel Read(const Surface& surface, int x, int y, int attachment = 0) {
        return ReadTarget(SurfaceAccess::Of(surface).target, x, y, attachment);
    }

    [[nodiscard]] Pixel ReadTarget(GpuTarget handle, int x, int y, int attachment = 0) {
        const GpuNativeTarget target = _device.Native(handle);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0 + static_cast<GLenum>(attachment));
        std::array<unsigned char, 4> value{};
        glReadPixels(x, target.height - 1 - y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, value.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        return {value[0], value[1], value[2], value[3]};
    }

    void Near(const std::string& name, int actual, int expected, int tolerance = 2) {
        const bool passed = std::abs(actual - expected) <= tolerance;
        std::cout << (passed ? "ok    " : "FAIL  ") << name << ": " << actual << " (expected " << expected << ")\n";
        if (!passed) ++_failures;
    }

    void Above(const std::string& name, int actual, int least) {
        const bool passed = actual >= least;
        std::cout << (passed ? "ok    " : "FAIL  ") << name << ": " << actual << " (at least " << least << ")\n";
        if (!passed) ++_failures;
    }

    [[nodiscard]] int Failures() const { return _failures; }

private:
    GlDevice _device;
    RenderPipeline _pipeline;
    int _failures = 0;
};

void MultiplyChangesOnlyTheColor(PixelCheck& check) {
    Surface image({64.0f, 64.0f}, {.emission = true});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image);
        paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(White, 1.0f));
        auto multiply = paint.Blending(Blend::Multiply);
        paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(Gray));
    });
    check.Near("multiply halves the colour", check.Read(image, 32, 32).r, 128);
    check.Near("multiply keeps the bloom source", check.Read(image, 32, 32, 1).r, 255);
}

// BeginPass opens every colour channel again while the device's blend cache still says Multiply.
void MultiplySurvivesANewPass(PixelCheck& check) {
    Surface first({64.0f, 64.0f}, {.emission = true});
    Surface second({64.0f, 64.0f}, {.emission = true});
    check.Frame([&](Painter& paint) {
        {
            auto into = paint.Target(first);
            paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(White, 1.0f));
        }
        {
            auto into = paint.Target(second);
            paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(White, 1.0f));
        }
        for (Surface* surface : {&first, &second}) {
            auto into = paint.Target(*surface, std::nullopt);
            auto multiply = paint.Blending(Blend::Multiply);
            paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(Gray));
        }
    });
    check.Near("first pass keeps the bloom source", check.Read(first, 32, 32, 1).r, 255);
    check.Near("next pass keeps the bloom source", check.Read(second, 32, 32, 1).r, 255);
}

void MultiplyLeavesLaterDrawingAlone(PixelCheck& check) {
    Surface image({64.0f, 64.0f}, {.emission = true});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image);
        {
            auto multiply = paint.Blending(Blend::Multiply);
            paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(Gray));
        }
        paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(White, 1.0f));
    });
    check.Near("emissive drawing after multiply reaches bloom", check.Read(image, 32, 32, 1).r, 255);
    check.Frame([&](Painter& paint) {
        auto multiplied = paint.Target(image, std::nullopt);
        auto multiply = paint.Blending(Blend::Multiply);
        paint.Rect({0.0f, 0.0f, 64.0f, 64.0f}, Fill(Gray));
    });
    check.Frame([&](Painter& paint) { auto cleared = paint.Target(image); });
    check.Near("a clear after multiply empties the bloom source", check.Read(image, 32, 32, 1).r, 0);
}

// Lights alone, on black, at full resolution: what the shader computes.
void LightFallsOffToItsRadius(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Light({.at = {64.0f, 64.0f}, .radius = 40.0f});
    });
    check.Near("light is full at its centre", check.Read(image, 64, 64).r, 255);
    // A pixel's centre is half a unit off, so these sit a little from the exact half.
    check.Near("light is half at half its radius", check.Read(image, 84, 64).r, 128, 8);
    check.Near("light is even in every direction", check.Read(image, 64, 44).r, check.Read(image, 83, 64).r, 2);
    check.Near("light is gone at its radius", check.Read(image, 104, 64).r, 0);
    check.Near("light adds nothing past its radius", check.Read(image, 120, 64).r, 0);
    check.Near("light writes no alpha", check.Read(image, 64, 64).a, 255);
}

void LightKeepsItsCoreAndColor(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Light({.at = {64.0f, 64.0f}, .radius = 40.0f, .color = {200, 100, 0, 255}, .core = 0.5f});
    });
    check.Near("core keeps full strength", check.Read(image, 80, 64).r, 200);
    check.Near("core keeps the colour", check.Read(image, 80, 64).g, 100);
    check.Near("past the core it fades", check.Read(image, 94, 64).r, 100, 10);
}

void LightsAddUp(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Rgb(0x202020));
        paint.Light({.at = {64.0f, 64.0f}, .radius = 40.0f, .color = {100, 0, 0, 255}});
        paint.Light({.at = {64.0f, 64.0f}, .radius = 40.0f, .color = {100, 0, 50, 255}});
    });
    const Pixel center = check.Read(image, 64, 64);
    check.Near("lights and ambient add up: red", center.r, 0x20 + 200);
    check.Near("lights and ambient add up: blue", center.b, 0x20 + 50);
}

void ConePointsWhereItIsTold(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    const LightSource down{.at = {64.0f, 64.0f},
                           .radius = 50.0f,
                           .cone = {.direction = Pi / 2.0f, .halfAngle = Pi / 4.0f, .softEdge = 0.1f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Light(down);
    });
    // y grows down, and angles run clockwise on screen: Pi / 2 points at the bottom of the image.
    check.Near("cone lights its direction", check.Read(image, 64, 84).r, 180, 40);
    check.Near("cone leaves the opposite side dark", check.Read(image, 64, 44).r, 0);
    check.Near("cone leaves its side dark", check.Read(image, 84, 64).r, 0);
    check.Near("cone is dark past its edge", check.Read(image, 84, 74).r, 0);

    // A camera turns the world, the cone with it: the light lands where the same world point is drawn.
    const CameraView quarterTurn{.offset = {64.0f, 64.0f}, .target = {64.0f, 64.0f}, .rotation = Pi / 2.0f};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        auto camera = paint.Camera(quarterTurn);
        paint.Light(down);
    });
    const Float2 lit = quarterTurn.ToScreen({64.0f, 84.0f});
    const Float2 dark = quarterTurn.ToScreen({64.0f, 44.0f});
    check.Near("cone turns with the camera", check.Read(image, static_cast<int>(lit.x), static_cast<int>(lit.y)).r, 180,
               40);
    check.Near("cone's far side turns too", check.Read(image, static_cast<int>(dark.x), static_cast<int>(dark.y)).r, 0);
}

void VisiblePolygonLimitsTheLight(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    constexpr Float2 Center{64.0f, 64.0f};
    // A notch: the wedge towards the right is blocked 10 units from the light.
    const std::array<Float2, 6> visible{Float2{24.0f, 24.0f}, {104.0f, 24.0f}, {74.0f, 54.0f},
                                        {74.0f, 74.0f},       {104.0f, 104.0f}, {24.0f, 104.0f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Light({.at = Center, .radius = 40.0f}, visible);
    });
    check.Near("inside the polygon falls off the same", check.Read(image, 44, 64).r, 128, 8);
    check.Near("the fan needs no centre seam", check.Read(image, 63, 64).r, 255, 2);
    check.Near("behind the notch is dark", check.Read(image, 84, 64).r, 0);
    check.Near("beside the notch is lit", check.Read(image, 74, 44).r, 150, 60);
}

void LayerDarkensTheWorld(PixelCheck& check) {
    Surface world({128.0f, 128.0f}, {.emission = true});
    const LightLayer layer({.resolution = 1.0f});
    const std::array lights{LightSource{.at = {30.0f, 30.0f}, .radius = 20.0f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, Black);
        paint.Rect({0.0f, 0.0f, 128.0f, 128.0f}, Fill(Gray));
        paint.Rect({100.0f, 100.0f, 28.0f, 28.0f}, Fill(White, 1.0f));
        layer.Draw(paint, {.ambient = Rgb(0x404040), .lights = lights});
    });
    check.Near("under the light the world keeps its colour", check.Read(world, 30, 30).r, 128);
    check.Near("the light stays where it was put", check.Read(world, 30, 98).r, 32, 2); // Not flipped
    check.Near("ambient darkens the rest", check.Read(world, 90, 30).r, 32, 2);
    check.Near("light never brightens past the colour", check.Read(world, 30, 30).g, 128);
    check.Near("emissive drawing is darkened in colour", check.Read(world, 110, 110).r, 64);
    check.Near("emissive drawing keeps its bloom source", check.Read(world, 110, 110, 1).r, 255);
}

void LayerFollowsTheCamera(PixelCheck& check) {
    Surface world({128.0f, 128.0f});
    const LightLayer half; // The default resolution: half the target
    const std::array lights{LightSource{.at = {1000.0f, 2000.0f}, .radius = 20.0f, .core = 0.5f}};
    const CameraView view{.offset = {40.0f, 80.0f}, .target = {1000.0f, 2000.0f}, .zoom = 1.5f};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        half.Draw(paint, {.ambient = Black, .lights = lights}, view);
    });
    check.Near("the light lands on its world point", check.Read(world, 40, 80).r, 255, 2);
    check.Near("the zoom scales its core", check.Read(world, 40 + 14, 80).r, 255, 6);
    check.Near("the zoom scales its reach", check.Read(world, 40 + 31, 80).r, 0, 2);
    check.Near("elsewhere stays dark", check.Read(world, 110, 20).r, 0);
}

void ViewportsDarkenOnlyTheirPart(PixelCheck& check) {
    Surface world({128.0f, 128.0f});
    const LightLayer layer({.resolution = 1.0f});
    const std::array left{LightSource{.at = {32.0f, 64.0f}, .radius = 16.0f}};
    const CameraView leftView{.viewport = {0.0f, 0.0f, 64.0f, 128.0f}};
    const CameraView rightView{.viewport = {64.0f, 0.0f, 64.0f, 128.0f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        layer.Draw(paint, {.ambient = Rgb(0x404040), .lights = left}, leftView);
    });
    check.Near("the viewport's light shows", check.Read(world, 32, 64).r, 255);
    check.Near("the viewport is dark around it", check.Read(world, 8, 8).r, 0x40, 2);
    check.Near("outside the viewport nothing changes", check.Read(world, 100, 64).r, 255);
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        layer.Draw(paint, {.ambient = Rgb(0x404040), .lights = left}, leftView);
        layer.Draw(paint, {.ambient = Rgb(0x202020)}, rightView);
    });
    check.Near("each view keeps its own frame: left", check.Read(world, 8, 8).r, 0x40, 2);
    check.Near("each view keeps its own frame: right", check.Read(world, 100, 64).r, 0x20, 2);
    check.Near("the first view's light survives the second", check.Read(world, 32, 64).r, 255);
}

// A new surface's texture is still bound when the first drawing lands in it, and a surface drawn twice in a frame is
// still bound from being shown. Both are feedback loops; the image must hold its drawing anyway.
void SurfaceShowsItsFirstDrawing(PixelCheck& check) {
    constexpr Float2 Size{128.0f, 128.0f};
    constexpr FloatRect Whole{0.0f, 0.0f, Size.x, Size.y};
    constexpr FloatRect Middle{32.0f, 32.0f, 64.0f, 64.0f};
    Surface world(Size);
    Surface image(Size);
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        {
            auto drawing = paint.Target(image, Black);
            paint.Rect(Middle, Fill(White));
        }
        paint.Image(image, Whole);
    });
    check.Near("a new surface of the same size shows its first drawing", check.Read(world, 64, 64).r, 255);
    check.Near("and only that drawing", check.Read(world, 8, 8).r, 0);

    Surface again(Size);
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        {
            auto drawing = paint.Target(again, Black);
            paint.Rect(Middle, Fill(White));
        }
        paint.Image(again, {0.0f, 0.0f, 64.0f, 128.0f});
        {
            auto drawing = paint.Target(again, Black);
            paint.Rect({64.0f, 32.0f, 64.0f, 64.0f}, Fill(White));
        }
        paint.Image(again, {64.0f, 0.0f, 64.0f, 128.0f});
    });
    check.Near("a surface drawn again shows the first drawing", check.Read(world, 32, 64).r, 255);
    check.Near("a surface drawn again shows the second drawing", check.Read(world, 96, 64).r, 255);
    check.Near("the second drawing replaced the first", check.Read(world, 70, 8).r, 0);
}

// The whole chain, through the canvas: bloom is computed after the light layer and still sees the emissive drawing.
void BloomShinesThroughTheDark(PixelCheck& check) {
    const LightLayer layer;
    const auto brightness = [&](bool withLayer) {
        const GpuTarget shown = check.Frame([&](Painter& paint) {
            paint.Post().bloom = {.intensity = 1.0f};
            paint.Rect({108.0f, 108.0f, 40.0f, 40.0f}, Fill(White, 1.0f));
            if (withLayer) layer.Draw(paint, {.ambient = Black});
        });
        return check.ReadTarget(shown, 128, 96).r; // Just outside the rectangle: only bloom reaches it
    };
    (void)brightness(true); // The canvas gains its bloom layer one frame after it is asked for
    const int lit = brightness(false);
    (void)brightness(true);
    const int dark = brightness(true);
    check.Above("bloom is visible at all", lit, 8);
    check.Near("bloom is the same in the dark", dark, lit, 2);
}
} // namespace

int main() {
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(WindowSize, WindowSize, "GPU pixel check");
    int failures = 0;
    {
        PixelCheck check;
        MultiplyChangesOnlyTheColor(check);
        MultiplySurvivesANewPass(check);
        MultiplyLeavesLaterDrawingAlone(check);
        SurfaceShowsItsFirstDrawing(check);
        LightFallsOffToItsRadius(check);
        LightKeepsItsCoreAndColor(check);
        LightsAddUp(check);
        ConePointsWhereItIsTold(check);
        VisiblePolygonLimitsTheLight(check);
        LayerDarkensTheWorld(check);
        LayerFollowsTheCamera(check);
        ViewportsDarkenOnlyTheirPart(check);
        BloomShinesThroughTheDark(check);
        failures = check.Failures();
    }
    CloseWindow();
    std::cout << "GPU pixel checks: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
