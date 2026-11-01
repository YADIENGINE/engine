#pragma once
#include <optional>
#include <string>
#include <vector>

// Where the Assets panel is looking: a folder, and optionally a set opened inside it.
struct AssetLocation {
    std::string folder;
    std::string set;
    bool operator==(const AssetLocation&) const = default;
};

// One step back: the location and the open scene before a move.
struct AssetBackStep {
    AssetLocation location;
    std::string scene; // Empty when no scene was open
};

// The Assets panel's back history. Browsing folders and opening graphics share one list, so each press of Back undoes
// the last thing the developer did there.
class AssetNavigation {
public:
    [[nodiscard]] const AssetLocation& Location() const { return _location; }
    // Moves to `next`; moving to the current location records nothing.
    void Go(const AssetLocation& next, const std::string& openScene);
    // A graphic is about to replace `openScene`; opening the scene already open records nothing.
    void SceneOpened(const std::string& openScene, const std::string& nextScene);
    // Fixes the location without history, when its folder is gone from the disk.
    void Correct(const AssetLocation& location) { _location = location; }
    [[nodiscard]] const AssetBackStep* Peek() const { return _steps.empty() ? nullptr : &_steps.back(); }
    // Restores the last step's location and returns the step, whose scene the caller reopens.
    std::optional<AssetBackStep> Back();

private:
    void Record(const std::string& openScene);
    AssetLocation _location;
    std::vector<AssetBackStep> _steps;
};
