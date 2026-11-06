#pragma once
#include <array>
#include <string_view>

enum class EditorTheme { Cabinet, Slate, Paper, NeonArcade, DeepOcean, SakuraInk, DesertTerminal, VampireVelvet, MatchaGarden, SolarFlare, ArcticLilac, MonochromeStudio, CandyCircuit };

struct EditorThemeDefinition {
    EditorTheme id;
    const char* key; // Stable preferences/CLI spelling; independent of enum values and display names.
    const char* title;
};

inline constexpr std::array EditorThemes{
    EditorThemeDefinition{EditorTheme::Cabinet, "cabinet", "Cabinet"},
    EditorThemeDefinition{EditorTheme::Slate, "slate", "Slate"},
    EditorThemeDefinition{EditorTheme::Paper, "paper", "Paper"},
    EditorThemeDefinition{EditorTheme::NeonArcade, "neon-arcade", "Neon Arcade"},
    EditorThemeDefinition{EditorTheme::DeepOcean, "deep-ocean", "Deep Ocean"},
    EditorThemeDefinition{EditorTheme::SakuraInk, "sakura-ink", "Sakura Ink"},
    EditorThemeDefinition{EditorTheme::DesertTerminal, "desert-terminal", "Desert Terminal"},
    EditorThemeDefinition{EditorTheme::VampireVelvet, "vampire-velvet", "Vampire Velvet"},
    EditorThemeDefinition{EditorTheme::MatchaGarden, "matcha-garden", "Matcha Garden"},
    EditorThemeDefinition{EditorTheme::SolarFlare, "solar-flare", "Solar Flare"},
    EditorThemeDefinition{EditorTheme::ArcticLilac, "arctic-lilac", "Arctic Lilac"},
    EditorThemeDefinition{EditorTheme::MonochromeStudio, "monochrome-studio", "Monochrome Studio"},
    EditorThemeDefinition{EditorTheme::CandyCircuit, "candy-circuit", "Candy Circuit"}};

[[nodiscard]] const EditorThemeDefinition& EditorThemeInfo(EditorTheme theme);
// External text is resolved at the settings/CLI boundary; missing and unknown keys use Cabinet.
[[nodiscard]] EditorTheme ParseEditorTheme(std::string_view key);
