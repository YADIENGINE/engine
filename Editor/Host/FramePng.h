#pragma once
#include <cstddef>
#include <vector>
#include "Editor/Internals/Platform/Windows/SharedFrames.h"

// PNG bytes of an RGBA frame (raylib's encoder; the editor's control endpoint writes them to its capture folder).
[[nodiscard]] std::vector<std::byte> EncodeFramePng(const PreviewPixels& frame);
