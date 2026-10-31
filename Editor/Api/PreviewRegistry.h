#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <array>
#include <optional>
#include "Editor/Api/EditorPreview.h"
#include "Editor/Api/PreviewServices.h"

struct PreviewBenchmarkVariant {
    std::string name;
    PreviewParameters parameters;
};
struct PreviewBenchmark {
    std::string id;
    int version = 1;
    std::array<PreviewBenchmarkVariant, 2> variants{{{"A", {}}, {"B", {}}}};
};
struct PreviewDefinition {
    std::string id;
    std::string name;
    std::string category;
    std::string description;
    std::vector<PreviewParameter> parameters;
    std::vector<PreviewAction> actions;
    std::function<std::unique_ptr<EditorPreview>(const PreviewServices&, const PreviewParameters&)> create;
    int categoryOrder = 0; // Lower values come first; use the same order for every entry in a category.
    std::optional<PreviewBenchmark> benchmark;
};

class PreviewRegistry {
public:
    PreviewRegistry();
    void Add(PreviewDefinition definition);
    [[nodiscard]] const std::vector<PreviewDefinition>& Entries() const { return _entries; }
    [[nodiscard]] const PreviewDefinition* Find(const std::string& id) const;

private:
    std::vector<PreviewDefinition> _entries;
};

[[nodiscard]] PreviewParameters PreviewDefaults(const PreviewDefinition& definition);
// Validate the entire snapshot before changing accepted values. OnReset recipe values remain staged.
[[nodiscard]] bool MergePreviewCapture(const PreviewDefinition& definition, const PreviewParameters& captured,
                                       PreviewParameters& accepted);
// Preserve only compatible fields after a catalog change; explain discarded overrides.
[[nodiscard]] PreviewParameters RestorePreviewParameters(const PreviewDefinition& definition,
                                                         const PreviewParameters& saved,
                                                         std::vector<std::string>& messages);
