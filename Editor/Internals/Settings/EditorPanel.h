#pragma once
#include <array>

enum class EditorPanel { Catalog, Viewport, Inspector, Output, Profiler, Count };
enum class EditorDockGroup { Left, Center, Right, Bottom };
enum class EditorWorkspaceLayout { Develop, Inspect, Profile, Custom };

struct EditorPanelDefinition {
    EditorPanel id;
    const char* title;
    const char* window;
    bool defaultVisible;
    EditorDockGroup dock;
};

inline constexpr std::array EditorPanels{
    EditorPanelDefinition{EditorPanel::Catalog, "Project", "Project###editor.catalog", true, EditorDockGroup::Left},
    EditorPanelDefinition{EditorPanel::Viewport, "View", "View###editor.viewport", true, EditorDockGroup::Center},
    EditorPanelDefinition{EditorPanel::Inspector, "Inspector", "Inspector###editor.inspector", true,
                          EditorDockGroup::Right},
    EditorPanelDefinition{EditorPanel::Output, "Output", "Output###editor.output", false, EditorDockGroup::Bottom},
    EditorPanelDefinition{EditorPanel::Profiler, "Profiler", "Profiler###editor.profiler", false,
                          EditorDockGroup::Bottom}};

struct EditorLayoutDefinition {
    EditorWorkspaceLayout id;
    const char* title;
};

inline constexpr std::array EditorLayouts{EditorLayoutDefinition{EditorWorkspaceLayout::Develop, "Develop"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Inspect, "Inspect"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Profile, "Profile"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Custom, "Last custom layout"}};

[[nodiscard]] constexpr auto DefaultEditorPanelVisibility() {
    std::array<bool, static_cast<std::size_t>(EditorPanel::Count)> visible{};
    for (const auto& panel : EditorPanels)
        visible[static_cast<std::size_t>(panel.id)] = panel.defaultVisible;
    return visible;
}

[[nodiscard]] constexpr const EditorPanelDefinition& EditorPanelInfo(EditorPanel panel) {
    return EditorPanels[static_cast<std::size_t>(panel)];
}
