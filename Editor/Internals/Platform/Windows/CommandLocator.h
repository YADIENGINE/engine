#pragma once
#include <string>

// Whether a terminal could start `program`: an existing path, or a name found on PATH with an executable or script
// extension (.exe, .cmd, .bat, .ps1). Used to say "not installed" before starting an agent CLI.
[[nodiscard]] bool CommandOnPath(const std::string& program);
