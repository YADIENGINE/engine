#pragma once
#include <vector>
#include "Graphics/Gpu/Internals/NullDevice.h"
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Paint/Internals/PaintContext.h"
#include "Graphics/Paint/Painter.hpp"
#include "Graphics/Render/Internals/FrameExecutor.h"
#include "Graphics/Render/Internals/RenderResources.h"

// A Painter over the NullDevice drawing into a canvas target: Begin, draw with 'paint', End, inspect 'device'.
struct PaintHarness {
    explicit PaintHarness(Float2 size = {400.0f, 200.0f}, float scale = 2.0f, LegacyHost* legacy = nullptr)
        : size(size)
        , scale(scale)
        , context(list, resources, executor, nullptr, legacy)
        , canvas(device.CreateTarget({static_cast<int>(size.x * scale), static_cast<int>(size.y * scale),
                                      GpuFormat::Rgba8, GpuFilter::Linear, false, nullptr})) {}

    void Begin(float pixelScale = 0.0f) {
        if (pixelScale > 0.0f) scale = pixelScale;
        device.BeginFrame();
        resources.BeginFrame({scale, scale});
        list.Reset({canvas, size, {0, 0, static_cast<int>(size.x * scale), static_cast<int>(size.y * scale)}, false},
                   PremultipliedColor{});
        device.ClearCommands();
    }
    void End() {
        list.Finish();
        FlushPaint(context);
        device.EndFrame();
        resources.EndFrame();
    }
    [[nodiscard]] std::vector<const NullCommand*> Draws() const {
        return device.CommandsOf(NullCommandType::DrawIndexed);
    }
    [[nodiscard]] std::vector<PaintVertex> Drawn(std::size_t draw = 0) const {
        return device.DrawnVertices(*Draws().at(draw));
    }

    Float2 size;
    float scale;
    NullDevice device;
    RenderResources resources{device};
    DrawList list;
    FrameExecutor executor{device};
    PaintContext context;
    Painter paint{context};
    GpuTarget canvas;
};
