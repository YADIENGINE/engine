#pragma once
#include "Editor/Api/SceneRegistry.h"

// Store metadata makers only; definitions are made when RunEditorWorker builds its catalog.
bool RegisterEditorScene(SceneDefinition (*make)());
