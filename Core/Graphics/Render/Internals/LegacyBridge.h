#pragma once
#include "Graphics/Paint/Internals/LegacyHost.h"

// Lets raylib-based drawing run inside a Painter frame during the migration (Painter::Legacy): binds the Painter's
// current target to raylib (with the canvas's logical projection, the Painter's transform and clip) and submits
// raylib's batch before the Painter draws again. Removed once nothing draws with raylib.
class LegacyBridge final : public LegacyHost {
public:
    LegacyBridge() = default;

    void Bind(const LegacyBinding& binding) override;
    void Flush() override;
    // End of frame: submits raylib's drawing and returns it to the window for EndDrawing.
    void Release();

private:
    bool _pushed = false; // A Painter target sits at the bottom of the raylib target stack
};
