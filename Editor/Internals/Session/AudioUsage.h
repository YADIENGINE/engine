#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

// The project's audio catalog as its worker reports it, so the editor can tell which sound files the game uses.
enum class AudioUseKind { Effect, Music };
struct AudioUse {
    AudioUseKind kind = AudioUseKind::Effect;
    std::size_t id = 0;
    std::string name;           // The catalog's optional name; empty shows the numeric id
    std::filesystem::path file; // Relative to the asset folder, as in the catalog
};
struct AudioUsage {
    bool reported = false; // False until a worker has sent the catalog: nothing counts as unused yet
    std::vector<AudioUse> uses;
};

// "Effect Click" or "Music #3".
[[nodiscard]] std::string AudioUseLabel(const AudioUse& use);
// Catalog entries that play the file; `file` is relative to the asset folder. Case does not matter (Windows paths).
[[nodiscard]] std::vector<const AudioUse*> AudioUsesOf(const AudioUsage& usage, const std::filesystem::path& file);
// One label per use, joined by ", "; empty when unused.
[[nodiscard]] std::string AudioUseLabels(const std::vector<const AudioUse*>& uses);
