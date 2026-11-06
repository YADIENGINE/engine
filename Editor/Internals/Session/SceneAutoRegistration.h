#pragma once

class SceneRegistry;

// Called once per worker catalog, before the project's explicit registrations.
void AddRegisteredCatalogScenes(SceneRegistry& registry);
