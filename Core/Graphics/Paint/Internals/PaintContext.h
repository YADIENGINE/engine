#pragma once
#include <cstdint>
#include <optional>
#include <vector>
#include "Graphics/Paint/Internals/DrawList.h"
#include "Graphics/Paint/Internals/ShapeArea.h"
#include "Graphics/Paint/Internals/Tessellator.h"
#include "Graphics/Paint/Painter.h"
#include "Graphics/Post/PostSettings.h"
#include "Graphics/Post/RenderQuality.h"

class FrameExecutor;
class RenderResources;
class GlyphCache;
class PixelAtlas;
class TextLayoutCache;
class Surface;

// Glyph rasterization and layout caching for Painter::Text (owned by the renderer).
struct TextServices {
    GlyphCache& glyphs;
    TextLayoutCache& layouts;
};

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
    PaintContext(DrawList& list, RenderResources& resources, FrameExecutor& executor, TextServices* text)
        : list(list)
        , resources(resources)
        , executor(executor)
        , text(text) {}

    DrawList& list;
    RenderResources& resources;
    FrameExecutor& executor;
    TextServices* text;             // Null: Painter::Text is not available
    const FontFace* font = nullptr; // The game's font (Painter::Font); borrowed, outlives the frames
    PixelAtlas* pixels = nullptr;   // Null: Painter::Sprite is not available
    int artPixel = 0;               // The project's artPixel: canvas units per art pixel; 0: a smooth project
    bool pixelUi = false;           // The project's pixelUi: a Stage pixelates its HUD too
    PaintFrameInfo frame;

    std::vector<PaintScopeRecord> scopes;
    MeshBuffer mesh;            // Tessellation scratch
    std::vector<Float2> points; // Transformed input points
    std::vector<Contour> contours;
    ShapeArea area;  // Fill rules, holes, glows and aligned strokes
    Regions regions; // The area's outlines and holes
    Regions band;    // A glow's or aligned stroke's band

    PaintContext(const PaintContext&) = delete;
    PaintContext& operator=(const PaintContext&) = delete;
};

// Executes the drawing recorded so far.
void FlushPaint(PaintContext& context);
