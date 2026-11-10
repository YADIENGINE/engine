#pragma once
#include <memory>
#include "Graphics/Surface/TextureRef.h"
#include "Utilities/Geometry/Float2.h"

struct SurfaceOptions {
    bool emission = false; // Keeps what is drawn into it as a bloom source too (composited with it)
    bool smooth = true;    // Linear filtering when drawn scaled; false: nearest (pixel art)
    // A pixel image: its size is in pixels (whole numbers, rounded up), whatever the canvas scale; nearest filtered
    // ('smooth' is ignored), and drawing into it has hard edges, no anti-aliasing (Docs/Graphics/PixelArt.md).
    bool pixels = false;
};

// Offscreen image: draw into it with Painter::Target, draw it with Painter::Image. Its size is logical (like the
// canvas); the device resolution follows the resolution of the target it is drawn from (the canvas's, or a pixel
// view's inside one), so it stays as sharp as that target. A pixel surface (SurfaceOptions::pixels) has exactly its
// size in pixels instead. GPU memory is allocated on first use and when the resolution changes; the contents are then
// lost (Painter::Stale tells when to redraw). A surface must not outlive the game's renderer (destroy it with the scene
// or view that owns it).
class Surface {
public:
    explicit Surface(Float2 size, SurfaceOptions options = {});
    ~Surface();

    // New size; the contents are lost.
    void Resize(Float2 size);
    [[nodiscard]] Float2 Size() const;
    [[nodiscard]] const SurfaceOptions& Options() const;
    // The current image; invalid until something was drawn into the surface.
    [[nodiscard]] TextureRef Texture() const;

private:
    friend struct SurfaceAccess;
    struct State;
    std::unique_ptr<State> _state;

public:
    Surface(Surface&&) noexcept;
    Surface& operator=(Surface&&) noexcept;
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;
};
