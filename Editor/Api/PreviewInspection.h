#pragma once
#include <optional>
#include <string>
#include <vector>

struct PreviewContentBounds {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool operator==(const PreviewContentBounds&) const = default;
};
enum class PreviewReadoutKind { Text, Number, Status };

struct PreviewReadout {
    std::string label;
    std::string value;
    PreviewReadoutKind kind = PreviewReadoutKind::Text;
    bool operator==(const PreviewReadout&) const = default;
};
struct PreviewTable {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> rows;
    bool operator==(const PreviewTable&) const = default;
};
// Read-only presentation data; never merged into the authored reset recipe.
struct PreviewInspection {
    std::optional<PreviewContentBounds> focus;
    std::vector<PreviewReadout> values;
    std::vector<PreviewTable> tables;
    std::vector<std::string> hiddenParameters;
    std::vector<std::string> disabledParameters;
    std::vector<std::string> disabledActions;
    bool showImage = true;
    bool operator==(const PreviewInspection&) const = default;
};
