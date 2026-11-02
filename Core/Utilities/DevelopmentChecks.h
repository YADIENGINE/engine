#pragma once

// Whether programmer errors stop the program (a throw at the call, graphics debug output) or are skipped and counted.
// An executable decides for itself: engine_add_game and engine_add_editor_worker compile
// Runtime/Internals/DevelopmentChecksBinding.cpp with its flags, so a development game keeps the checks even when the
// engine libraries it links were built optimized. Elsewhere the value follows how the engine itself was built.
[[nodiscard]] bool DevelopmentChecks();
void SetDevelopmentChecks(bool enabled);
