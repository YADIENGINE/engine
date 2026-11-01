#pragma once
#include <filesystem>
#include "Audio/AudioSystem.h"

// `--audio-report REQUEST`: answers `python Engine audio` with the project's audio catalog and facts about sound files,
// without a window or an audio device. The request and the answer are JSON files (Docs/AudioTools.md).
void WriteAudioReport(const std::filesystem::path& request, const AudioCatalog& catalog);
