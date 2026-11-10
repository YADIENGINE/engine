// Opt-in check of what the GPU really draws: a hidden window renders with the real device and reads the pixels back.
// The NullDevice tests see vertices and commands, not shaders or GL state; these cover that gap. Not a CTest: it needs
// a desktop/GPU, like EngineEditorUiSmoke.
//   EngineGpuPixelCheck [--compose-cost]
#include "external/glad.h"
#include <raylib.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>
#include <vector>
#include "Graphics/Gpu/Internals/GlDevice.h"
#include "Graphics/Light/LightLayer.h"
#include "Graphics/Light/LightOccluders.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Paint/Path.h"
#include "Graphics/Render/Internals/PostChain.h"
#include "Graphics/Pixel/PixelCanvas.h"
#include "Graphics/Pixel/PixelPalette.h"
#include "Graphics/Pixel/PixelSprite.h"
#include "Graphics/PixelView/PixelView.h"
#include "Graphics/Render/Internals/RenderPipeline.h"
#include "Graphics/Surface/Internals/SurfaceState.h"
#include "Graphics/Surface/Surface.h"
#include "Graphics/Text/FontFace.h"
#include "Graphics/Text/TextStyle.h"
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
    GpuTarget Frame(const std::function<void(Painter&)>& draw, double time = 0.0) {
        Painter& paint = _pipeline.BeginFrame({WindowSize, WindowSize}, time, 0.0f);
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

    // Every pixel of a target, rows from the top, RGBA.
    [[nodiscard]] std::vector<Pixel> ReadAll(GpuTarget handle) {
        const GpuNativeTarget target = _device.Native(handle);
        std::vector<unsigned char> bytes(static_cast<std::size_t>(target.width) * target.height * 4);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, target.framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, target.width, target.height, GL_RGBA, GL_UNSIGNED_BYTE, bytes.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        std::vector<Pixel> pixels(static_cast<std::size_t>(target.width) * target.height);
        for (int y = 0; y < target.height; ++y)
            for (int x = 0; x < target.width; ++x) {
                const std::size_t from = (static_cast<std::size_t>(target.height - 1 - y) * target.width + x) * 4;
                pixels[static_cast<std::size_t>(y) * target.width + x] = {bytes[from], bytes[from + 1], bytes[from + 2],
                                                                          bytes[from + 3]};
            }
        return pixels;
    }

    void SetQuality(const RenderQuality& quality) { _pipeline.SetQuality(quality); }
    [[nodiscard]] GlDevice& Device() { return _device; }

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

void PolygonEdgesStayOnTheOutline(PixelCheck& check) {
    Surface image({64.0f, 64.0f});
    const std::array<Float2, 4> square{Float2{20.0f, 20.0f}, {40.0f, 20.0f}, {40.0f, 40.0f}, {20.0f, 40.0f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Polygon(square, Fill(White));
    });
    check.Near("a polygon's first pixel inside is full", check.Read(image, 20, 30).r, 255);
    check.Near("a polygon does not grow past its outline", check.Read(image, 19, 30).r, 0);
    check.Near("nor on the far side", check.Read(image, 40, 30).r, 0);
}

// A snow cap that thins to nothing at both ends: its thin parts used to turn inside out and draw a wedge below it.
void ThinPolygonsDrawNoWedge(PixelCheck& check) {
    Surface image({200.0f, 80.0f});
    const auto crest = [](float x) { return 40.0f + 15.0f * std::sin(x * 0.05f); };
    std::vector<Float2> cap;
    for (int i = 0; i <= 40; ++i)
        cap.push_back({static_cast<float>(i) * 5.0f, crest(static_cast<float>(i) * 5.0f)});
    for (int i = 40; i >= 0; --i) {
        const float x = static_cast<float>(i) * 5.0f;
        cap.push_back({x, crest(x) + 3.0f * std::pow(std::sin(Pi * x / 200.0f), 4.0f)});
    }
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Polygon(cap, Fill(White));
    });
    check.Near("the cap's thick middle is drawn", check.Read(image, 100, static_cast<int>(crest(100.0f) + 1.5f)).r,
               255);
    int stray = 0;
    for (int x = 2; x < 198; x += 4) {
        const int below = static_cast<int>(crest(static_cast<float>(x)) + 3.0f) + 3;
        const int above = static_cast<int>(crest(static_cast<float>(x))) - 3;
        if (below < 80) stray = std::max(stray, check.Read(image, x, below).r);
        if (above >= 0) stray = std::max(stray, check.Read(image, x, above).r);
    }
    check.Near("nothing is drawn away from the thin cap", stray, 0);
}

void PathHolesStayEmpty(PixelCheck& check) {
    Surface image({128.0f, 128.0f});
    Path ring;
    ring.SetFillRule(FillRule::EvenOdd).AddCircle({64.0f, 64.0f}, 40.0f).AddCircle({64.0f, 64.0f}, 20.0f);
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.FillPath(ring, Fill(White));
    });
    check.Near("the ring is filled", check.Read(image, 94, 64).r, 255);
    check.Near("its hole stays empty", check.Read(image, 64, 64).r, 0);
    check.Near("and so does the outside", check.Read(image, 64, 10).r, 0);
}

void AlignedBordersSitOutsideOrInside(PixelCheck& check) {
    Surface image({128.0f, 64.0f});
    const std::array<Float2, 4> left{Float2{20.0f, 20.0f}, {44.0f, 20.0f}, {44.0f, 44.0f}, {20.0f, 44.0f}};
    const std::array<Float2, 4> right{Float2{84.0f, 20.0f}, {108.0f, 20.0f}, {108.0f, 44.0f}, {84.0f, 44.0f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Polygon(left, {.fill = Gray, .stroke = {4.0f, White, StrokeAlign::Outside}});
        paint.Polygon(right, {.fill = Gray, .stroke = {4.0f, White, StrokeAlign::Inside}});
    });
    check.Near("an outside border lies outside", check.Read(image, 17, 32).r, 255);
    check.Near("and leaves the fill whole", check.Read(image, 21, 32).r, 128);
    check.Near("an inside border lies inside", check.Read(image, 86, 32).r, 255);
    check.Near("and leaves the outside empty", check.Read(image, 82, 32).r, 0);
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

// Everything through the layer at full resolution, on white under black ambient: a pixel shows only the light it gets.
void LitBy(PixelCheck& check, Surface& world, const LightSource& light, const LightOccluders& occluders) {
    const LightLayer layer({.resolution = 1.0f, .softness = 0.0f});
    const std::array lights{light};
    const std::array<const LightOccluders*, 1> sets{&occluders};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        layer.Draw(paint, {.ambient = Black, .lights = lights, .occluders = sets});
    });
}

void WallsCastShadows(PixelCheck& check) {
    constexpr LightSource Light{.at = {30.0f, 64.0f}, .radius = 90.0f, .core = 0.8f};
    Surface world({128.0f, 128.0f});
    {
        LightOccluders wall;
        const std::vector<Float2> line{{64.0f, 34.0f}, {64.0f, 94.0f}};
        wall.Line(line);
        LitBy(check, world, Light, wall);
        check.Near("before a wall is lit", check.Read(world, 60, 64).r, 255);
        check.Near("behind a wall is dark", check.Read(world, 70, 64).r, 0);
        check.Near("far behind a wall is dark", check.Read(world, 100, 64).r, 0);
        check.Near("past the wall's end is lit", check.Read(world, 70, 16).r, 255);
        check.Near("a shadow widens with distance", check.Read(world, 100, 14).r, 0);
    }
    {
        LightOccluders box;
        const std::vector<Float2> outline{{64.0f, 44.0f}, {84.0f, 44.0f}, {84.0f, 84.0f}, {64.0f, 84.0f}};
        box.Outline(outline);
        LitBy(check, world, Light, box);
        check.Near("a self-lit outline is lit inside", check.Read(world, 74, 64).r, 255);
        check.Near("its shadow starts behind it", check.Read(world, 90, 64).r, 0);
        box.Clear();
        box.Outline(outline, {}, false);
        LitBy(check, world, Light, box);
        check.Near("otherwise it is dark inside", check.Read(world, 74, 64).r, 0);
        check.Near("and lit before it", check.Read(world, 60, 64).r, 255);
    }
    {
        LightOccluders around;
        const std::vector<Float2> outline{{20.0f, 54.0f}, {40.0f, 54.0f}, {40.0f, 74.0f}, {20.0f, 74.0f}};
        around.Outline(outline);
        LitBy(check, world, Light, around);
        check.Near("a light inside an outline is not stopped by it", check.Read(world, 80, 64).r, 255);
    }
    {
        LightOccluders corners; // Two boxes touching at a corner, on the line from the light through it
        const std::vector<Float2> one{{44.0f, 64.0f}, {64.0f, 64.0f}, {64.0f, 84.0f}, {44.0f, 84.0f}};
        const std::vector<Float2> two{{64.0f, 44.0f}, {84.0f, 44.0f}, {84.0f, 64.0f}, {64.0f, 64.0f}};
        corners.Outline(one);
        corners.Outline(two);
        LitBy(check, world, {.at = {30.0f, 30.0f}, .radius = 140.0f, .core = 0.9f}, corners);
        check.Near("no light leaks between boxes that touch", check.Read(world, 74, 74).r, 0);
        check.Near("nor further along that line", check.Read(world, 96, 96).r, 0);
    }
    {
        LightOccluders wall;
        const std::vector<Float2> line{{64.0f, 34.0f}, {64.0f, 94.0f}};
        wall.Line(line);
        LightSource free = Light;
        free.shadows = false;
        LitBy(check, world, free, wall);
        check.Near("a light without shadows passes", check.Read(world, 70, 64).r, 255);
    }
}

// The fan's falloff must equal the plain light's, or a light would jump as a wall comes into its reach.
void AFanFallsOffLikeAPlainLight(PixelCheck& check) {
    constexpr LightSource Light{.at = {64.0f, 64.0f}, .radius = 50.0f};
    Surface plain({128.0f, 128.0f});
    Surface shadowed({128.0f, 128.0f});
    const LightOccluders none;
    LightOccluders far; // In reach, and hiding only its own small corner
    const std::vector<Float2> line{{108.0f, 108.0f}, {112.0f, 104.0f}};
    far.Line(line);
    LitBy(check, plain, Light, none);
    LitBy(check, shadowed, Light, far);
    for (const int x : {64, 70, 80, 90, 100, 110})
        check.Near("same falloff at x " + std::to_string(x), check.Read(shadowed, x, 64).r, check.Read(plain, x, 64).r,
                   1);
}

// A wall's shadow along y = 64 past x = 64, on white under black ambient.
void SoftenedBy(PixelCheck& check, Surface& world, const LightLayer& layer, const CameraView& view) {
    constexpr LightSource Light{.at = {30.0f, 64.0f}, .radius = 120.0f, .core = 0.8f};
    LightOccluders wall;
    const std::vector<Float2> line{{64.0f, 64.0f}, {64.0f, 200.0f}};
    wall.Line(line);
    const std::array lights{Light};
    const std::array<const LightOccluders*, 1> sets{&wall};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        layer.Draw(paint, {.ambient = Black, .lights = lights, .occluders = sets}, view);
    });
}

void ShadowEdgesAreSoft(PixelCheck& check) {
    Surface world({128.0f, 128.0f});
    const CameraView still{};
    const LightLayer layer({.resolution = 1.0f, .softness = 8.0f});
    SoftenedBy(check, world, layer, still);
    check.Near("far from a soft edge on the lit side", check.Read(world, 100, 50).r, 255);
    check.Near("on a soft edge it is half lit", check.Read(world, 100, 64).r, 128, 30);
    check.Above("the edge falls gradually", check.Read(world, 100, 61).r - check.Read(world, 100, 64).r, 20);
    check.Above("and on", check.Read(world, 100, 64).r - check.Read(world, 100, 67).r, 20);
    check.Near("far from a soft edge in the shadow", check.Read(world, 100, 78).r, 0);
    check.Near("light does not go far through a wall", check.Read(world, 80, 100).r, 0);

    const LightLayer hard({.resolution = 1.0f, .softness = 0.0f});
    SoftenedBy(check, world, hard, still);
    check.Near("softness 0: lit just before the edge", check.Read(world, 100, 62).r, 255);
    check.Near("softness 0: dark just after it", check.Read(world, 100, 66).r, 0);
}

// A wide spread passes twice; the nine samples of one pass alone would show as steps.
void AWideSoftEdgeHasNoSteps(PixelCheck& check) {
    Surface world({128.0f, 128.0f});
    const LightLayer layer({.resolution = 1.0f, .softness = 30.0f});
    SoftenedBy(check, world, layer, {});
    int largestStep = 0;
    int total = 0;
    for (int y = 40; y < 90; ++y) {
        const int step = std::abs(check.Read(world, 100, y).r - check.Read(world, 100, y + 1).r);
        largestStep = std::max(largestStep, step);
        total += step;
    }
    check.Above("a wide edge goes all the way down", total, 200);
    check.Near("and has no jump", largestStep, 0, 22);
}

void FlashesLightTheWorld(PixelCheck& check) {
    Surface world({128.0f, 128.0f});
    const LightLayer layer({.resolution = 1.0f});
    const std::array flashes{LightSource{.at = {30.0f, 64.0f}, .radius = 40.0f, .core = 0.9f}};
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(world, White);
        layer.Draw(paint, {.ambient = Black, .flashes = {.ambient = Gray, .lights = flashes}});
    });
    check.Near("a whole-scene flash raises the ambient", check.Read(world, 110, 110).r, 128);
    check.Near("a flash with a place adds its light", check.Read(world, 30, 64).r, 255);
}

// Not a check: what shadows and softening add on this GPU, at a common size, with half-resolution light.
void ReportCost(PixelCheck& check) {
    constexpr Float2 Size{1280.0f, 720.0f};
    constexpr int Frames = 120;
    Surface world(Size);
    std::array<LightSource, 6> lights{};
    for (std::size_t i = 0; i < lights.size(); ++i)
        lights[i] = {.at = {200.0f + static_cast<float>(i) * 170.0f, 360.0f}, .radius = 300.0f};
    LightOccluders walls;
    for (int i = 0; i < 12; ++i) {
        const float x = 100.0f + static_cast<float>(i) * 100.0f;
        const std::vector<Float2> line{{x, 200.0f}, {x + 40.0f, 240.0f}};
        walls.Line(line);
    }
    const std::array<const LightOccluders*, 1> sets{&walls};
    const auto cost = [&](const char* name, const LightLayer& layer, bool shadows) {
        GLuint query = 0;
        glGenQueries(1, &query);
        GLuint64 total = 0;
        for (int frame = 0; frame < Frames; ++frame) {
            glBeginQuery(GL_TIME_ELAPSED, query);
            check.Frame([&](Painter& paint) {
                auto into = paint.Target(world, White);
                LightFrame light{.ambient = Black, .lights = lights};
                if (shadows) light.occluders = sets;
                layer.Draw(paint, light);
            });
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 elapsed = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed);
            if (frame >= Frames / 4) total += elapsed;
        }
        glDeleteQueries(1, &query);
        std::cout << "cost  " << name << ": " << static_cast<double>(total) / (Frames - Frames / 4) / 1e6
                  << " ms per frame\n";
    };
    const LightLayer soft;
    const LightLayer hard({.softness = 0.0f});
    cost("no shadows", soft, false);
    cost("hard shadows", hard, true);
    cost("soft shadows", soft, true);
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

void DrawStripes(Painter& paint) {
    paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill(Black));
    for (int x = 0; x < 256; x += 32)
        paint.Rect({static_cast<float>(x), 0.0f, 16.0f, 256.0f}, Fill(White));
}

// How many pixels of a row differ from another image's.
int RowDifference(PixelCheck& check, GpuTarget image, const std::vector<int>& reference, int y) {
    int different = 0;
    for (int x = 0; x < WindowSize; ++x)
        different += std::abs(check.ReadTarget(image, x, y).r - reference[static_cast<std::size_t>(x)]) > 8 ? 1 : 0;
    return different;
}

std::vector<int> Stripes(PixelCheck& check, int y) {
    const GpuTarget plain = check.Frame([](Painter& paint) { DrawStripes(paint); });
    std::vector<int> row;
    for (int x = 0; x < WindowSize; ++x)
        row.push_back(check.ReadTarget(plain, x, y).r);
    return row;
}

struct Spread {
    double mean = 0.0;
    double deviation = 0.0;
};

// Over a square of pixels, 'side' wide, at (x, y).
Spread SpreadOf(PixelCheck& check, GpuTarget image, int x, int y, int side) {
    std::vector<int> values;
    for (int row = 0; row < side; ++row)
        for (int column = 0; column < side; ++column)
            values.push_back(check.ReadTarget(image, x + column, y + row).r);
    Spread spread;
    for (const int value : values)
        spread.mean += value;
    spread.mean /= static_cast<double>(values.size());
    for (const int value : values)
        spread.deviation += (value - spread.mean) * (value - spread.mean);
    spread.deviation = std::sqrt(spread.deviation / static_cast<double>(values.size()));
    return spread;
}

// All the new settings at rest: the old finish comes out as the formula says, and no finish at all leaves the canvas.
void OldFinishIsUntouched(PixelCheck& check) {
    const GpuTarget untouched = check.Frame([](Painter& paint) {
        paint.Post().tint.color = Rgb(0x304060); // A colour at amount 0
        paint.Post().wobble.speed = 3.0f;
        paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill(Gray));
    });
    check.Near("nothing asked: the canvas itself", check.ReadTarget(untouched, 128, 128).r, 128, 0);
    const GpuTarget vignette = check.Frame([](Painter& paint) {
        paint.Post().vignette = 0.5f;
        paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill(White));
    });
    check.Near("vignette leaves the middle", check.ReadTarget(vignette, 128, 128).r, 255, 0);
    check.Near("vignette darkens a corner", check.ReadTarget(vignette, 1, 1).r, 128, 2);
    const GpuTarget lines = check.Frame([](Painter& paint) {
        paint.Post().scanlines = 1.0f;
        paint.Post().scanlineCount = 4.0f;
        paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill(White));
    });
    check.Near("a scanline's dark middle", check.ReadTarget(lines, 128, 0).r, 0, 3);
    check.Near("a scanline's light middle", check.ReadTarget(lines, 128, 32).r, 255, 3);
}

void ScreenRegionLimitsTheFinish(PixelCheck& check) {
    const auto frame = [&](FloatRect region) {
        return check.Frame([&](Painter& paint) {
            paint.Post().region = region;
            paint.Post().fade = 1.0f;
            paint.Post().scanlines = 1.0f;
            paint.Post().scanlineCount = 2.0f;
            paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill({255, 0, 0, 255}));
        });
    };
    const GpuTarget monitor = frame({32.0f, 64.0f, 128.0f, 128.0f});
    check.Near("region leaves the outside colour", check.ReadTarget(monitor, 16, 96).r, 255, 0);
    check.Near("region leaves the outside green", check.ReadTarget(monitor, 16, 96).g, 0, 0);
    check.Near("region scanline starts at its own edge", check.ReadTarget(monitor, 96, 64).r, 0, 2);
    check.Near("region scanline spans its own height", check.ReadTarget(monitor, 96, 96).r, 76, 2);
    check.Near("region fades its own colour", check.ReadTarget(monitor, 96, 96).g, 76, 2);

    // The clipped region has zero size. Real GLSL must return before dividing by that size.
    const GpuTarget outside = frame({300.0f, 300.0f, 40.0f, 40.0f});
    for (const int coordinate : {0, 128, 255}) {
        const Pixel pixel = check.ReadTarget(outside, coordinate, coordinate);
        check.Near("off-canvas region passes red through", pixel.r, 255, 0);
        check.Near("off-canvas region passes green through", pixel.g, 0, 0);
    }
}

void FadeDrainsColour(PixelCheck& check) {
    const auto red = [&](float fade) {
        const GpuTarget shown = check.Frame([&](Painter& paint) {
            paint.Post().fade = fade;
            paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill({255, 0, 0, 255}));
        });
        return check.ReadTarget(shown, 128, 128);
    };
    const Pixel half = red(0.5f);
    check.Near("half a fade: red", half.r, 166);
    check.Near("half a fade: green", half.g, 38);
    const Pixel full = red(1.0f);
    check.Near("full fade: red is its brightness", full.r, 76);
    check.Near("full fade: green matches", full.g, 76);
    check.Near("full fade: blue matches", full.b, 76);
}

void TintMultiplies(PixelCheck& check) {
    const auto white = [&](float amount) {
        const GpuTarget shown = check.Frame([&](Painter& paint) {
            paint.Post().tint = {.color = Rgb(0x6496c8), .amount = amount};
            paint.Rect({0.0f, 0.0f, 256.0f, 256.0f}, Fill(White));
        });
        return check.ReadTarget(shown, 128, 128);
    };
    const Pixel full = white(1.0f);
    check.Near("full tint: red", full.r, 0x64);
    check.Near("full tint: green", full.g, 0x96);
    check.Near("full tint: blue", full.b, 0xc8);
    check.Near("half a tint: red", white(0.5f).r, (255 + 0x64) / 2);
}

void GrainMovesAndFavoursTheDark(PixelCheck& check) {
    const auto frame = [&](double time) {
        return check.Frame(
            [](Painter& paint) {
                paint.Post().grain.amount = 0.5f;
                paint.Rect({0.0f, 0.0f, 128.0f, 256.0f}, Fill(Rgb(0x181818)));
                paint.Rect({128.0f, 0.0f, 128.0f, 256.0f}, Fill(Rgb(0xe8e8e8)));
            },
            time);
    };
    GpuTarget image = frame(1.0);
    const Spread dark = SpreadOf(check, image, 16, 16, 64);
    const Spread light = SpreadOf(check, image, 176, 16, 64);
    check.Above("grain shows in the dark (deviation x10)", static_cast<int>(dark.deviation * 10.0), 40);
    check.Above("grain is stronger in the dark (x10)", static_cast<int>((dark.deviation - light.deviation * 1.5) * 10.0),
                1);
    check.Near("grain keeps a mid brightness", static_cast<int>(light.mean), 0xe8, 4);

    // The same pixels at the same moment are the same; another moment or another place are different.
    std::vector<int> first;
    for (int x = 0; x < 64; ++x)
        first.push_back(check.ReadTarget(image, x, 40).r);
    image = frame(1.0);
    int repeated = 0;
    for (int x = 0; x < 64; ++x)
        repeated += check.ReadTarget(image, x, 40).r == first[static_cast<std::size_t>(x)] ? 1 : 0;
    check.Near("the same moment gives the same grain", repeated, 64, 0);
    image = frame(1.0 + 1.0 / 24.0);
    int same = 0;
    for (int x = 0; x < 64; ++x)
        same += check.ReadTarget(image, x, 40).r == first[static_cast<std::size_t>(x)] ? 1 : 0;
    check.Above("the next moment gives other grain (different of 64)", 64 - same, 32);
    int sameRow = 0;
    for (int x = 0; x < 64; ++x)
        sameRow += check.ReadTarget(image, x, 41).r == check.ReadTarget(image, x, 40).r ? 1 : 0;
    check.Above("the next row gives other grain (different of 64)", 64 - sameRow, 32);
}

void WobbleBendsTheMiddleOnly(PixelCheck& check) {
    const std::vector<int> plain = Stripes(check, 128);
    const std::vector<int> edge = Stripes(check, 0);
    const auto bent = [&](float amount, double time) {
        return check.Frame(
            [&](Painter& paint) {
                paint.Post().wobble = {.amount = amount, .speed = 1.0f};
                DrawStripes(paint);
            },
            time);
    };
    (void)bent(8.0f, 2.0);
    const GpuTarget image = bent(8.0f, 4.0);
    check.Above("the middle row moves", RowDifference(check, image, plain, 128), 8);
    check.Near("the top edge stays still", RowDifference(check, image, edge, 0), 0, 0);
    int leftEdge = 0;
    for (int y = 0; y < WindowSize; y += 4)
        leftEdge += std::abs(check.ReadTarget(image, 0, y).r - 255) > 8 ? 1 : 0;
    check.Near("the left edge stays still", leftEdge, 0, 0);
}

void TapeBandRollsAndLinesJump(PixelCheck& check) {
    const std::vector<int> plain = Stripes(check, 128);
    const auto taped = [&](const TapeSettings& tape, double time) {
        return check.Frame(
            [&](Painter& paint) {
                paint.Post().tape = tape;
                DrawStripes(paint);
            },
            time);
    };
    const auto bandRow = [&](GpuTarget image) {
        int best = -1;
        int bestDifference = -1;
        for (int y = 0; y < WindowSize; y += 2) {
            const int difference = RowDifference(check, image, plain, y);
            if (difference > bestDifference) {
                bestDifference = difference;
                best = y;
            }
        }
        return best;
    };
    const TapeSettings band{.band = 1.0f};
    (void)taped(band, 10.0);
    GpuTarget image = taped(band, 12.0); // 2 seconds: 14% of the way, from the bottom of the image up
    const int first = bandRow(image);
    const int away = (first + WindowSize / 2) % WindowSize;
    check.Above("the band pushes its row", RowDifference(check, image, plain, first), 32);
    check.Near("a row far from the band is untouched", RowDifference(check, image, plain, away), 0, 0);
    image = taped(band, 14.0);
    const int later = bandRow(image);
    check.Above("the band has rolled (rows moved)", std::abs(later - first), 16);

    const TapeSettings lines{.shift = 8.0f};
    (void)taped(lines, 20.0);
    image = taped(lines, 21.0);
    int changedRows = 0;
    for (int y = 0; y < WindowSize; y += 2)
        changedRows += RowDifference(check, image, plain, y) > 0 ? 1 : 0;
    check.Above("lines jump sideways (of 128 rows)", changedRows, 32);
}

void PlayerSwitchStopsWhatMoves(PixelCheck& check) {
    check.SetQuality({.screenMotion = false});
    const auto draw = [](Painter& paint) {
        paint.Post().grain.amount = 1.0f;
        paint.Post().wobble.amount = 16.0f;
        paint.Post().tape = {.band = 1.0f, .shift = 8.0f};
        DrawStripes(paint);
    };
    const std::vector<int> plain = Stripes(check, 128);
    const GpuTarget still = check.Frame(draw, 3.0);
    check.Near("switch off: the row is as drawn", RowDifference(check, still, plain, 128), 0, 0);
    const GpuTarget faded = check.Frame(
        [&](Painter& paint) {
            draw(paint);
            paint.Post().tint = {.color = Rgb(0x808080), .amount = 1.0f};
        },
        3.0);
    check.Near("switch off: the tint stays", check.ReadTarget(faded, 8, 128).r, 128);
    check.SetQuality({});
}

// What the finish adds to the compose pass, measured on the GPU at full HD. Prints; never fails.
void MeasureComposeCost() {
    constexpr int Width = 1920;
    constexpr int Height = 1080;
    constexpr int Rounds = 240;
    GlDevice device;
    PostChain chain(device);
    const GpuTarget canvas = device.CreateTarget({Width, Height, GpuFormat::Rgba8, GpuFilter::Linear, false, "Canvas"});
    GLuint query = 0;
    glGenQueries(1, &query);
    const auto cost = [&](const char* name, const PostSettings& post) {
        double total = 0.0;
        int counted = 0;
        for (int i = 0; i < Rounds; ++i) {
            device.BeginFrame();
            glBeginQuery(GL_TIME_ELAPSED, query);
            (void)chain.Run(canvas, Width, Height, static_cast<float>(Height), post, RenderQuality{},
                            static_cast<double>(i) / 60.0);
            glEndQuery(GL_TIME_ELAPSED);
            device.EndFrame();
            GLuint64 elapsed = 0;
            glGetQueryObjectui64v(query, GL_QUERY_RESULT, &elapsed);
            if (i >= Rounds / 4) { // The first rounds warm up
                total += static_cast<double>(elapsed);
                ++counted;
            }
        }
        std::cout << name << ": " << total / counted / 1000.0 << " us\n";
    };
    std::cout << "GPU: " << reinterpret_cast<const char*>(glGetString(GL_RENDERER)) << ", " << Width << " x " << Height
              << "\n";
    PostSettings post;
    post.vignette = 0.001f;
    cost("warming up", post);
    cost("compose, one old effect (vignette)", post);
    PostSettings fade = post;
    fade.fade = 0.5f;
    cost("+ fade", fade);
    PostSettings tint = post;
    tint.tint = {.color = Rgb(0x8c9ccc), .amount = 0.8f};
    cost("+ tint", tint);
    PostSettings grain = post;
    grain.grain.amount = 0.3f;
    cost("+ grain", grain);
    PostSettings wobble = post;
    wobble.wobble.amount = 6.0f;
    cost("+ wobble", wobble);
    PostSettings tape = post;
    tape.tape = {.band = 0.7f, .shift = 2.0f};
    cost("+ tape", tape);
    PostSettings all = post;
    all.fade = 0.7f;
    all.tint = tint.tint;
    all.grain = grain.grain;
    all.wobble = wobble.wobble;
    all.tape = tape.tape;
    all.aberration = 1.5f;
    cost("all of them, with aberration", all);
    glDeleteQueries(1, &query);
    device.DestroyTarget(canvas);
}
} // namespace

// Each ink shows its palette colour exactly, with hard edges, and a flash moves every ink toward its colour.
void SpriteShowsItsPalette(PixelCheck& check) {
    PixelCanvas canvas(4, 2);
    canvas.FlatRect(0, 0, 2, 2, 1);
    canvas.FlatRect(2, 0, 2, 2, 2);
    const PixelSprite sprite(canvas);
    const PixelPalette palette{Color4{200, 40, 10, 255}, Color4{0, 100, 250, 255}};
    const PixelPalette night = palette.Mixed(Black, 0.5f);
    Surface image({64.0f, 64.0f});
    check.Frame([&](Painter& paint) {
        auto into = paint.Target(image, Black);
        paint.Sprite(sprite, palette, {0.0f, 0.0f}, {.scale = 8.0f});
        paint.Sprite(sprite, night, {0.0f, 20.0f}, {.scale = 8.0f});
        paint.Sprite(sprite, palette, {0.0f, 40.0f}, {.scale = 8.0f, .flash = White, .flashAmount = 1.0f});
    });
    check.Near("ink 1 is its red", check.Read(image, 4, 4).r, 200, 0);
    check.Near("ink 2 is its blue", check.Read(image, 20, 4).b, 250, 0);
    check.Near("the last column of ink 1 is still ink 1", check.Read(image, 15, 15).g, 40, 0);
    check.Near("the first column of ink 2 is already ink 2", check.Read(image, 16, 0).g, 100, 0);
    check.Near("ink 0 shows what is behind", check.Read(image, 40, 4).r, 0, 0);
    check.Near("another palette draws the same sprite", check.Read(image, 4, 24).r, 100, 1);
    check.Near("a full flash is the flash colour", check.Read(image, 20, 44).r, 255, 0);
}

// In a pixel surface shapes, strokes and text cover a pixel fully or not at all: no blended edge pixels.
void PixelSurfacesHaveHardEdges(PixelCheck& check) {
    const FontFace font = FontFace::FromFile(ENGINE_TEST_FONT);
    const auto partlyCovered = [&](const Surface& image) {
        int partial = 0;
        for (const Pixel& pixel : check.ReadAll(SurfaceAccess::Of(image).target))
            if (pixel.a != 0 && pixel.a != 255) ++partial;
        return partial;
    };
    Surface image({96.0f, 64.0f}, {.pixels = true});
    Surface smooth({96.0f, 64.0f}); // The same drawing anti-aliased, to show the check can see blended pixels
    const auto draw = [&](Painter& paint, Surface& surface) {
        auto into = paint.Target(surface, std::nullopt);
        paint.Circle({16.3f, 16.6f}, 11.4f, Fill(White));
        paint.Polygon(std::array{Float2{34.2f, 3.1f}, Float2{60.7f, 9.9f}, Float2{41.5f, 29.3f}}, Fill(White));
        paint.RoundRect({66.4f, 4.2f, 24.0f, 20.5f}, 5.3f, {.stroke = {1.6f, White}});
        paint.Line({4.0f, 36.0f}, {50.0f, 61.0f}, {.width = 0.4f, .color = White});
        paint.Polyline(std::array{Float2{4.0f, 60.0f}, Float2{30.0f, 40.0f}, Float2{60.0f, 58.0f}},
                       {.width = 2.3f, .color = White, .join = StrokeJoin::Round});
        paint.Text(font, "Pixel", {58.0f, 36.0f}, {.size = 14.0f, .color = White});
    };
    check.Frame([&](Painter& paint) {
        draw(paint, image);
        draw(paint, smooth);
    });
    check.Above("an anti-aliased surface has partly covered pixels", partlyCovered(smooth), 50);
    int partial = 0;
    int covered = 0;
    for (const Pixel& pixel : check.ReadAll(SurfaceAccess::Of(image).target)) {
        if (pixel.a != 0 && pixel.a != 255) ++partial;
        if (pixel.a == 255) ++covered;
    }
    check.Near("a pixel surface has no partly covered pixels", partial, 0, 0);
    check.Above("and the shapes are there", covered, 800);
}

// A pixel view on a canvas scaled by a fraction still gives every art pixel the same whole number of window pixels.
void PixelViewPixelsAreEvenAtAFractionalScale(PixelCheck& check) {
    constexpr Vec2 Window{256, 144};
    RenderPipeline pipeline(check.Device(), {.canvasSize = {240.0f, 135.0f}, .artPixel = 3});
    PixelView view;
    Painter& paint = pipeline.BeginFrame(Window, 0.0, 0.0f); // 1.0667 window pixels per unit: 3.2 per art pixel
    {
        const CameraView camera{.offset = {120.0f, 67.5f}, .target = {1.3f, 0.0f}};
        auto into = view.Begin(paint, camera, Black);
        auto world = paint.Camera(view.Shown());
        for (int column = -60; column < 60; column += 2) // Art pixel columns, every other one white
            paint.Rect({static_cast<float>(column) * 3.0f, -30.0f, 3.0f, 60.0f}, Fill(White));
    }
    view.Present(paint);
    const GpuTarget shown = pipeline.FinishCanvas();
    const std::vector<Pixel> pixels = check.ReadAll(shown);
    (void)pipeline.BeginWindow();
    check.Near("the art pixel is a whole number of window pixels", view.Layout().scale, 3, 0);
    // One row through the middle: every run of one colour away from the edges is exactly 'scale' pixels long.
    const int y = Window.y / 2;
    std::vector<int> runs;
    int run = 1;
    for (int x = 1; x < Window.x; ++x) {
        const bool same = (pixels[static_cast<std::size_t>(y) * Window.x + x].r > 127) ==
                          (pixels[static_cast<std::size_t>(y) * Window.x + x - 1].r > 127);
        if (same) {
            ++run;
        } else {
            runs.push_back(run);
            run = 1;
        }
    }
    int uneven = 0;
    for (std::size_t i = 1; i < runs.size(); ++i) // The first run may be cut by the canvas edge
        if (runs[i] != 3) ++uneven;
    pipeline.EndFrame();
    check.Above("columns across the row", static_cast<int>(runs.size()), 70);
    check.Near("art pixels of uneven width", uneven, 0, 0);
}

int main(int argc, char** argv) {
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
        PolygonEdgesStayOnTheOutline(check);
        ThinPolygonsDrawNoWedge(check);
        PathHolesStayEmpty(check);
        AlignedBordersSitOutsideOrInside(check);
        VisiblePolygonLimitsTheLight(check);
        WallsCastShadows(check);
        AFanFallsOffLikeAPlainLight(check);
        LayerDarkensTheWorld(check);
        LayerFollowsTheCamera(check);
        ViewportsDarkenOnlyTheirPart(check);
        BloomShinesThroughTheDark(check);
        ShadowEdgesAreSoft(check);
        AWideSoftEdgeHasNoSteps(check);
        FlashesLightTheWorld(check);
        OldFinishIsUntouched(check);
        ScreenRegionLimitsTheFinish(check);
        FadeDrainsColour(check);
        TintMultiplies(check);
        GrainMovesAndFavoursTheDark(check);
        WobbleBendsTheMiddleOnly(check);
        TapeBandRollsAndLinesJump(check);
        PlayerSwitchStopsWhatMoves(check);
        SpriteShowsItsPalette(check);
        PixelSurfacesHaveHardEdges(check);
        PixelViewPixelsAreEvenAtAFractionalScale(check);
        failures = check.Failures();
        ReportCost(check);
    }
    if (argc > 1 && std::string(argv[1]) == "--compose-cost") MeasureComposeCost();
    CloseWindow();
    std::cout << "GPU pixel checks: " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
