#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>

enum class SoundId : std::size_t {};
enum class MusicId : std::size_t {};
enum class MusicVolumeGroup { Default, Menu };

struct EffectAsset {
    SoundId id;
    std::filesystem::path file;
};
struct MusicAsset {
    MusicId id;
    std::filesystem::path file;
    MusicVolumeGroup volumeGroup = MusicVolumeGroup::Default;
};
// Paths are relative to the application's asset root.
struct AudioCatalog {
    std::vector<EffectAsset> effects;
    std::vector<MusicAsset> music;
};
