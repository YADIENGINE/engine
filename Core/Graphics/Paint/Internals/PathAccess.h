#pragma once
#include <vector>
#include "Graphics/Paint/Path.hpp"

// Engine-side reading of a Path's verbs and points.
struct PathAccess {
    using Verb = Path::Verb;
    [[nodiscard]] static const std::vector<Verb>& Verbs(const Path& path) { return path._verbs; }
    [[nodiscard]] static const std::vector<Float2>& Points(const Path& path) { return path._points; }
};
