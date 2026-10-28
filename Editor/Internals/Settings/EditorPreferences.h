#pragma once
#include <array>
#include <string>
#include <vector>
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"

inline constexpr std::array EditorFontSizes{75, 100, 125, 150};
enum class EditorPanel { Catalog, Viewport, Inspector, Output, Profiler, Count };

struct EditorPreferences {
    std::array<bool, static_cast<std::size_t>(EditorPanel::Count)> visible{true, true, true, true, false};
    std::string docking;
    std::vector<std::string> favorites;
    std::vector<std::string> recent;
    EditorLogFilter outputFilter;
    bool followOutput = true;
    bool showMetrics = false;
    [[nodiscard]] bool& Visible(EditorPanel panel) { return visible[static_cast<std::size_t>(panel)]; }
    [[nodiscard]] bool IsFavorite(const std::string& id) const;
    void ToggleFavorite(const std::string& id);
    void Opened(const std::string& id);
    bool operator==(const EditorPreferences&) const = default;
};
