#pragma once
#include <string_view>
#include <vector>
#include "Runtime/ProjectSettings.h"
#include "Tuning/TuningFile.h"

// The project settings a program runs with (Docs/ProjectSettings.md). A game or worker build compiles
// Game/Game.settings.json into a RegisterProjectSettings call that runs before main; development builds then read the
// file once more at the first CurrentProjectSettings, so an edit needs only a restart. A program without one (tests,
// tools) runs with the schema's defaults.
void RegisterProjectSettings(ProjectSettings compiled, bool readFile);
[[nodiscard]] const ProjectSettings& CurrentProjectSettings();

// The parsed ProjectSettings schema (ProjectSettings::Schema), for the editor and the values reader.
[[nodiscard]] const std::vector<TuningField>& ProjectSettingsSchema();
// The settings a values file describes: its values, the defaults for the rest. Throws TuningFormatError.
[[nodiscard]] ProjectSettings ReadProjectSettings(std::string_view text);
