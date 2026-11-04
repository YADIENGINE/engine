#pragma once
#include <array>

// Preferences save panels by their key (EditorPanelDefinition::key), so the order here is free to change.
// clang-format off
enum class EditorPanel { Catalog, Viewport, Inspector, Output, Profiler, Console, Assets, AudioPreview, Count };
// clang-format on
enum class EditorDockGroup { Left, Center, Right, Bottom };
// Saved as numbers: new layouts go last. Agent: the scene beside a tall Console for an agent session.
enum class EditorWorkspaceLayout { Develop, Inspect, Profile, Custom, Agent };

struct EditorPanelDefinition {
    EditorPanel id;
    const char* key; // In saved preferences
    const char* title;
    const char* window;
    bool defaultVisible;
    EditorDockGroup dock;
};

inline constexpr std::array EditorPanels{
    EditorPanelDefinition{EditorPanel::Catalog, "catalog", "Scenes", "Scenes###editor.catalog", true,
                          EditorDockGroup::Left},
    EditorPanelDefinition{EditorPanel::Viewport, "viewport", "Viewport", "Viewport###editor.viewport", true,
                          EditorDockGroup::Center},
    EditorPanelDefinition{EditorPanel::Inspector, "inspector", "Inspector", "Inspector###editor.inspector", true,
                          EditorDockGroup::Right},
    EditorPanelDefinition{EditorPanel::Output, "output", "Output", "Output###editor.output", false,
                          EditorDockGroup::Bottom},
    EditorPanelDefinition{EditorPanel::Profiler, "profiler", "Profiler", "Profiler###editor.profiler", false,
                          EditorDockGroup::Bottom},
    EditorPanelDefinition{EditorPanel::Console, "console", "Console", "Console###editor.console", false,
                          EditorDockGroup::Bottom},
    EditorPanelDefinition{EditorPanel::Assets, "assets", "Assets", "Assets###editor.assets", true,
                          EditorDockGroup::Bottom},
    EditorPanelDefinition{EditorPanel::AudioPreview, "audio-preview", "Audio Preview",
                          "Audio Preview###editor.audio-preview", false, EditorDockGroup::Bottom}};

// Alt dock'un panelleri (Ctrl+J son kullanılan paneli bağımsız açıp kapatır).
inline constexpr std::array EditorBottomPanels{EditorPanel::Output, EditorPanel::Profiler, EditorPanel::Console,
                                               EditorPanel::Assets, EditorPanel::AudioPreview};

struct EditorLayoutDefinition {
    EditorWorkspaceLayout id;
    const char* title;
};

inline constexpr std::array EditorLayouts{EditorLayoutDefinition{EditorWorkspaceLayout::Develop, "Develop"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Inspect, "Inspect"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Profile, "Profile"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Agent, "Agent"},
                                          EditorLayoutDefinition{EditorWorkspaceLayout::Custom, "Last custom layout"}};

using EditorPanelVisibility = std::array<bool, static_cast<std::size_t>(EditorPanel::Count)>;

[[nodiscard]] constexpr EditorPanelVisibility DefaultEditorPanelVisibility() {
    EditorPanelVisibility visible{};
    for (const auto& panel : EditorPanels)
        visible[static_cast<std::size_t>(panel.id)] = panel.defaultVisible;
    return visible;
}

[[nodiscard]] constexpr const EditorPanelDefinition& EditorPanelInfo(EditorPanel panel) {
    return EditorPanels[static_cast<std::size_t>(panel)];
}
