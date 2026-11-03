#pragma once
#include <filesystem>
#include <string>

// Whether a terminal could start `program`: an existing path, or a name found on PATH with an executable or script
// extension (.exe, .cmd, .bat, .ps1). Used to say "not installed" before starting an agent CLI.
[[nodiscard]] bool CommandOnPath(const std::string& program);
// git.exe from PATH, then its usual install folders: a launcher started before Git was installed passes on an old PATH.
// The current directory is never searched, so a project cannot supply its own git. Empty when not found.
[[nodiscard]] std::filesystem::path FindGitExecutable();
