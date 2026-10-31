#pragma once
#include <optional>
#include "Graphics/Gpu/Internals/GpuTypes.h"
#include "Utilities/Affine2.h"

// Where raylib-based drawing should go while a Painter::Legacy scope is open.
struct LegacyBinding {
    GpuNativeTarget target; // framebuffer 0: the window
    Float2 size;            // Logical size of the target
    PixelRect viewport;
    Affine2 transform;
    std::optional<PixelRect> scissor; // Target device pixels, origin top-left
    friend bool operator==(const LegacyBinding&, const LegacyBinding&) = default;
};

// The raylib side of the migration bridge (implemented next to the window; tests use a fake).
class LegacyHost {
public:
    virtual ~LegacyHost() = default;
    // Prepares raylib to draw into 'binding' (target, projection, transform, clip, alpha blending).
    virtual void Bind(const LegacyBinding& binding) = 0;
    // Submits raylib's pending drawing before the Painter's next drawing executes.
    virtual void Flush() = 0;

protected:
    LegacyHost() = default;

public:
    LegacyHost(const LegacyHost&) = delete;
    LegacyHost& operator=(const LegacyHost&) = delete;
};
