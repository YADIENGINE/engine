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
```
<!-- /recipe -->

Pitfalls: Use a reference to Tuning<T>() when reading multiple fields. Release uses compiled values; adding/removing fields requires a build. Each target needs unique tuning filenames.

Concept: `Docs/Tuning.md#setup`.
