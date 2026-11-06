#include "Editor/Api/SceneAutoRegistration.h"
#include <vector>
#include "Editor/Internals/Session/SceneAutoRegistration.h"

namespace {
auto& SceneMakers() {
    static std::vector<SceneDefinition (*)()> makers;
    return makers;
}
} // namespace

bool RegisterCatalogScene(SceneDefinition (*make)()) {
    SceneMakers().push_back(make);
    return true;
}

void AddRegisteredCatalogScenes(SceneRegistry& registry) {
    for (const auto make : SceneMakers())
        registry.Add(make());
}
