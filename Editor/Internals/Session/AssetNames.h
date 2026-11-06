#pragma once
#include <string>
#include <vector>

enum class AssetNameKind { Folder, Graphic };

// Why the Assets panel cannot create `name`, as one short sentence; empty when it can. The same rules as
// `python Engine asset new` / `mkdir`, so the form explains a name before the command would refuse it. `taken`: names
// already used in the target folder, compared without case as Windows does.
[[nodiscard]] std::string AssetNameProblem(AssetNameKind kind, const std::string& name,
                                           const std::vector<std::string>& taken);
// The reason a failed `python Engine asset ...` run printed: its last line, without the "error: " prefix.
[[nodiscard]] std::string AssetCommandError(const std::string& output);
