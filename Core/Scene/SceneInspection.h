#pragma once
#include <optional>
#include <string>
#include <vector>

struct SceneContentBounds {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool operator==(const SceneContentBounds&) const = default;
};
enum class SceneReadoutKind { Text, Number, Status };

struct SceneReadout {
    std::string label;
    std::string value;
    SceneReadoutKind kind = SceneReadoutKind::Text;
    bool operator==(const SceneReadout&) const = default;
};
struct SceneTable {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    bool operator==(const SceneTable&) const = default;
};
// Read-only presentation data; never merged into the authored reset recipe.
struct SceneInspection {
    std::optional<SceneContentBounds> focus;
    std::vector<SceneReadout> values;
    std::vector<SceneTable> tables;
    std::vector<std::string> hiddenParameters;
    std::vector<std::string> disabledParameters;
    std::vector<std::string> disabledActions;
    bool showImage = true;
    bool operator==(const SceneInspection&) const = default;
};
