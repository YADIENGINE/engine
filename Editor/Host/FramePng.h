#pragma once
#include <cstddef>
#include <vector>
#include "Editor/Internals/Session/FrameCapture.h"

// The editor window as drawn so far this frame (call after the UI has rendered, before the buffers swap).
[[nodiscard]] PreviewPixels ReadWindowPixels();
// PNG bytes of an RGBA frame with optional labels (raylib's encoder and default font, so it needs the editor's GL
// context; the control endpoint writes the bytes to its capture folder).
[[nodiscard]] std::vector<std::byte> EncodeFramePng(const PreviewPixels& frame,
                                                    const std::vector<FrameCaptureLabel>& labels);
