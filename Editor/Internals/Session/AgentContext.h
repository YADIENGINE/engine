#pragma once
#include <deque>
#include <filesystem>
#include <string>
#include <vector>
#include "Editor/Api/SceneRegistry.h"
#include "Editor/Internals/Diagnostics/EditorDiagnostics.h"
#include "Utilities/Vec2.h"

// Context lines the editor sends to the Console's agent session (Docs/Editor/Usage.md → Agent workspace). Each starts
// with "[editor]" like a Mark line, so an agent recognises editor context, and ends ready for the developer's words.
// Parameters are listed only when they differ from their initial values.

// " k=v k=v" for changed parameters, values as JSON; empty when none changed.
[[nodiscard]] std::string ChangedParameters(const SceneDefinition& definition, const SceneParameters& values);
// The open scene: id, playback time, a capture of the whole frame and the changed parameters.
[[nodiscard]] std::string SceneContext(const SceneDefinition& definition, const SceneParameters& values, double seconds,
                                       bool paused, Vec2 canvas, const std::filesystem::path& image);
// One Inspector parameter: its value, range or choices and description.
[[nodiscard]] std::string ParameterContext(const SceneDefinition& definition, const SceneParameter& parameter,
                                           const SceneValue& value);
// Output lines, one per line, after a header; at most `limit`, keeping the newest.
[[nodiscard]] std::string OutputContext(const std::vector<EditorLogEntry>& entries, std::size_t limit);
// The last Build & Refresh failure: result, exit code and its error lines (at most `limit`); empty when it did not
// fail.
[[nodiscard]] std::string BuildFailureContext(const EditorBuildAttempt& attempt,
                                              const std::deque<EditorLogEntry>& attemptEntries, std::size_t limit);

inline constexpr std::size_t AgentContextMaxLines = 40;
// Sent after BuildFailureContext by "Ask agent to fix".
inline constexpr const char* AgentFixBuildRequest = "Fix these build errors, then run `python Engine refresh`.";
