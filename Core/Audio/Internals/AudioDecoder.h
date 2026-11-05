#pragma once
#include <filesystem>
#include "Audio/AudioFiles.h"

// miniaudio's decoders; they need no audio device.
[[nodiscard]] DecodedAudio DecodeAudioSamples(const std::filesystem::path& file);
// The file's length; 0 when it cannot say.
[[nodiscard]] float AudioFileSeconds(const std::filesystem::path& file);
