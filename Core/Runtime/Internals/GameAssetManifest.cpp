#include "Runtime/Internals/GameAssetManifest.h"
#include "Runtime/GameSettings.h"

std::vector<std::filesystem::path> GameAssetManifest(const GameSettings& settings) {
    const AudioCatalog audio = CatalogWithSounds(settings.audio, settings.sounds);
    auto assets = settings.assets;
    for (const auto& effect : audio.effects)
        assets.push_back(effect.file);
    for (const auto& music : audio.music)
        assets.push_back(music.file);
    return assets;
}
