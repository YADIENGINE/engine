#include "Runtime/Internals/GameAssetManifest.h"
#include <algorithm>
#include "Runtime/GameSettings.h"

std::vector<std::filesystem::path> GameAssetManifest(const GameSettings& settings) {
    const AudioCatalog audio = CatalogWithSounds(settings.audio, settings.sounds);
    auto assets = settings.assets;
    const auto add = [&assets](const std::filesystem::path& file) {
        if (std::find(assets.begin(), assets.end(), file) == assets.end()) assets.push_back(file);
    };
    const auto& font = settings.project.font;
    if (!font.empty()) add(std::filesystem::path(std::u8string_view(reinterpret_cast<const char8_t*>(font.data()), font.size())));
    for (const auto& fallback : settings.fontFallbacks)
        add(fallback);
    for (const auto& effect : audio.effects)
        assets.push_back(effect.file);
    for (const auto& music : audio.music)
        assets.push_back(music.file);
    return assets;
}
