// [recipe:TuningValue]
#include "AgentDocs/Recipes/Code/TuningValue.h"
#include "Character.tuning.h"
#include "Tuning/Tuning.h"

// Generated from AgentDocs/Recipes/Code/Character.tuning.json by engine_add_tuning.
float RecipeRadius() { return Tuning<CharacterTuning>().radius; }
// [/recipe]
