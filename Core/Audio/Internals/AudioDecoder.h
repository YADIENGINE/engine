#pragma once
#include <filesystem>
#include "Audio/AudioFiles.h"

// raylib's decoders; they need no audio device.
[[nodiscard]] DecodedAudio DecodeAudioSamples(const std::filesystem::path& file);
