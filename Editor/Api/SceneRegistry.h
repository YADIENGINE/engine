#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include <limits>
#include "Editor/Api/EditorScene.h"
#include "Editor/Api/SceneServices.h"

inline constexpr const char* WorkSceneCategory = "Work";
inline constexpr int WorkSceneCategoryOrder = std::numeric_limits<int>::lowest();

struct SceneBenchmarkVariant {
    std::string name;
    SceneParameters parameters;
};
struct SceneBenchmark {
    std::string id;
    int version = 1;
    std::array<SceneBenchmarkVariant, 2> variants{{{"A", {}}, {"B", {}}}};
};
// Marks a catalog entry as an asset: the Assets panel shows it instead of the scene catalog.
struct SceneAssetInfo {
    std::string file; // Registration source, as AssetPathKey
    std::string set;
    std::string item; // Empty for a single drawing
    std::string usage;
    bool operator==(const SceneAssetInfo&) const = default;
};
struct SceneDefinition {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::vector<SceneParameter> parameters;
    std::vector<SceneAction> actions;
    std::function<std::unique_ptr<EditorScene>(const SceneServices&, const SceneParameters&)> create;
    int categoryOrder = 0; // Lower values come first; use the same order for every entry in a category.
    std::optional<SceneBenchmark> benchmark;
    std::optional<SceneAssetInfo> asset;
};

class SceneRegistry {
public:
    SceneRegistry();
    void Add(SceneDefinition definition);
    [[nodiscard]] const std::vector<SceneDefinition>& Entries() const { return _entries; }
    [[nodiscard]] const SceneDefinition* Find(const std::string& id) const;

private:
    std::vector<SceneDefinition> _entries;
};

[[nodiscard]] SceneParameters SceneDefaults(const SceneDefinition& definition);
// Validate the entire snapshot before changing accepted values. OnReset recipe values remain staged.
[[nodiscard]] bool MergeSceneCapture(const SceneDefinition& definition, const SceneParameters& captured,
                                     SceneParameters& accepted);
// Preserve only compatible fields after a catalog change; explain discarded overrides.
[[nodiscard]] SceneParameters RestoreSceneParameters(const SceneDefinition& definition, const SceneParameters& saved,
                                                     std::vector<std::string>& messages);
