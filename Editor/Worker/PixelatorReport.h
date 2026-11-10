#pragma once
#include <string>
#include "Editor/Internals/Pixelator/PixelatorAsset.h"

// The worker's answer to the editor's "pixels" request: the registered pixel graphic with catalog `id`, its items and
// its frames sampled from GraphicAsset::pixels (PixelFrameSampling.h). A graphic that is unknown, has no pixels or
// throws while sampling comes back with `error` set.
[[nodiscard]] PixelatorAsset PixelatorReport(const std::string& id);
