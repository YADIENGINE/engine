#pragma once
#include <filesystem>

// `--pixel-lint REQUEST`: answers `python Engine asset lint` without a window or an editor. The request names an
// answer file and catalog ids ({"out", "ids": [...]}; an id without "#item" means every item of the set, none means
// every pixel graphic); the answer is {"version", "reports": [PixelatorLintReport...], "unknown": [ids]}.
void WritePixelLintReport(const std::filesystem::path& request);
