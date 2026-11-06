// [recipe:CharacterScene-header]
#pragma once
#include "Editor/Api/SceneRegistry.h"

// The game's own scene (RecipeScene) in the editor's catalog: no second copy for the editor.
[[nodiscard]] SceneDefinition RecipeCharacterDefinition();
// [/recipe]
