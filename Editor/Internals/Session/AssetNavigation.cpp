#include "Editor/Internals/Session/AssetNavigation.h"

namespace {
constexpr std::size_t MaxSteps = 128;
} // namespace

void AssetNavigation::Record(const std::string& openScene) {
    if (_steps.size() == MaxSteps) _steps.erase(_steps.begin());
    _steps.push_back({_location, openScene});
}

void AssetNavigation::Go(const AssetLocation& next, const std::string& openScene) {
    if (next == _location) return;
    Record(openScene);
    _location = next;
}

void AssetNavigation::SceneOpened(const std::string& openScene, const std::string& nextScene) {
    if (openScene != nextScene) Record(openScene);
}

std::optional<AssetBackStep> AssetNavigation::Back() {
    if (_steps.empty()) return std::nullopt;
    auto step = std::move(_steps.back());
    _steps.pop_back();
    _location = step.location;
    return step;
}
