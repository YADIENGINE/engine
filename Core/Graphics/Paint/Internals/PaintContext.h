#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Paint/Internals/LegacyHost.h"
#include "Graphics/Paint/Internals/Tessellator.h"
#include "Graphics/Paint/Painter.hpp"
#include "Graphics/Post/PostSettings.hpp"
#include "Graphics/Post/RenderQuality.hpp"

class FrameExecutor;
class RenderResources;
class GlyphCache;
class Surface;

struct PaintFrameInfo {
    double time = 0.0;
    float delta = 0.0f;
    RenderQuality quality;
    PostSettings post;
};

// Open Target / Group scope: what its end has to finish.
struct PaintScopeRecord {
    std::uint32_t depth = 0;
    Surface* surface = nullptr; // Target
    GpuTarget transient{};      // Group
    PaintTarget parent;         // Group: the target it composites into
    GroupStyle group;
};

// Everything a Painter draws with during one frame. Owned by the renderer (or a test); plain data: the behaviour is
// in Painter and FrameExecutor.
struct PaintContext {
    PaintContext(DrawList& list, RenderResources& resources, FrameExecutor& executor, GlyphCache* glyphs,
                 LegacyHost* legacy)
        : list(list)
        , resources(resources)
        , executor(executor)
        , glyphs(glyphs)
        , legacy(legacy) {}

    DrawList& list;
    RenderResources& resources;
    FrameExecutor& executor;
    GlyphCache* glyphs; // Null: text is not available (tests without fonts)
    LegacyHost* legacy; // Null: Painter::Legacy is not available
    PaintFrameInfo frame;

    // Legacy bridge state.
    bool legacyDirty = false; // Raylib may hold drawing the GPU has not seen yet
    std::optional<LegacyBinding> legacyBound;
    std::uint64_t legacyRevision = 0;
    std::uint32_t legacySwitches = 0;

    std::vector<PaintScopeRecord> scopes;
    MeshBuffer mesh;            // Tessellation scratch
    std::vector<Float2> points; // Transformed input points
    std::vector<Contour> contours;

    PaintContext(const PaintContext&) = delete;
    PaintContext& operator=(const PaintContext&) = delete;
};

// Executes the drawing recorded so far, after raylib's pending drawing (legacy bridge) if there is any.
void FlushPaint(PaintContext& context);
