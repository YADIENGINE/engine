#pragma once
#include <string>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"

// The worker's answer to the editor's "pixels" request: the registered pixel graphic with catalog `id`, its items and
// the frames GraphicAsset::pixels declares (PixelatorFrames.h). A graphic that is unknown, has no pixels or throws
// comes back with `error` set.
[[nodiscard]] PixelatorAsset PixelatorReport(const std::string& id);
