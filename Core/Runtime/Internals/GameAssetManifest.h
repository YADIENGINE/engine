#pragma once
#include <filesystem>
#include <vector>

struct GameSettings;

// Every file the settings name: sound tables, catalog and `assets`. `release` packages the sounds on this list and
// leaves the others out (Docs/Release.md), so anything a game plays has to come through here.
[[nodiscard]] std::vector<std::filesystem::path> GameAssetManifest(const GameSettings& settings);
