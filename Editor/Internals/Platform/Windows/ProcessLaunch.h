#pragma once
#include <map>
#include <string>
#include <vector>
#include "Editor/Internals/Process/ProcessCommand.h"

// What CreateProcessW needs: the quoted command line, and this process's environment with the overrides applied.
[[nodiscard]] std::wstring ProcessCommandLine(const ProcessCommand& command);
[[nodiscard]] std::vector<wchar_t> ProcessEnvironmentBlock(const std::map<std::string, std::string>& overrides);
