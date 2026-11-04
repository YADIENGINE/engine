Read a generated tuning value from a tuning JSON file.

APIs: Core/Tuning/Tuning.h; generated Character.tuning.h.

Source: AgentDocs/Recipes/Code/Character.tuning.json. Add engine_add_tuning(MyGame ROOT "${CMAKE_CURRENT_SOURCE_DIR}") to the owning target; the generated include root is supplied automatically.

<!-- recipe:TuningValue -->
```cpp
#include "AgentDocs/Recipes/Code/TuningValue.h"
#include "Character.tuning.h"
#include "Tuning/Tuning.h"

// Generated from AgentDocs/Recipes/Code/Character.tuning.json by engine_add_tuning.
float RecipeRadius() { return Tuning<CharacterTuning>().radius; }
// A "choice" field is a generated enum class named after the field.
bool RecipeRuns() { return Tuning<CharacterTuning>().gait == CharacterTuning::Gait::Run; }
```
<!-- /recipe -->

Pitfalls: Use a reference to Tuning<T>() when reading multiple fields. Release uses compiled values; adding/removing fields requires a build. Each target needs unique tuning filenames. A `choice` field is a generated `enum class` (`CharacterTuning::Gait::Run`); add a choice in the JSON, never in C++. Write what a value does in its `note`: the Inspector shows it and the header carries it as a comment. Values are tuned in the editor's Inspector; edit the JSON only to add fields.

Concept: `Docs/Tuning.md#setup`.
