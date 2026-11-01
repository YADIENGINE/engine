---
name: graphic-asset
description: Draw a new character, enemy, item, prop, icon, background or other standalone graphic and register it in the Assets panel, with variants in one set and a viewport preview.
---

# Draw and register a graphic

Commands run from the project root. Rule: [Asset library](../../../Docs/Design/AssetLibrary.md#authoring-rule).
Example: [Graphic asset](../../Recipes/GraphicAsset.md).

1. **Decide:** a new standalone graphic (character, enemy, item, prop, icon or background) draws only from parameters
   and time, without reading a game model. Write it as `Game/Assets/Graphics/<Folder>/X.h`, `X.cpp` and `X.asset.cpp`.
   Keep a model-reading View in place; extract any stateless drawing part into Graphics and call it from the View.
   Variants (color, kind) are `items` in one set, not separate assets. Existing code need not move. The project's own
   agent file takes precedence if it specifies another location.
2. **Skeleton:** `python Engine asset new X --folder Characters` generates the three files; choose a suitable folder.
3. **Draw:** implement the drawing in `X.cpp`, fit it to `box`, and take time as a parameter. Fill `items` for variants,
   supply meaningful `usage` (including the variant via `{item}`), and set `thumbnailTime` when the initial frame is
   empty or unrepresentative. Keep editor registration in `X.asset.cpp`, drawing callable by game code in `X.h`.
4. **Refresh:** `python Engine refresh`. If no editor is open, follow the
   [visual-change skill](../visual-change/SKILL.md)'s no-editor path; do not launch it unless asked.
5. **Preview:** `python Engine ctl find NAME`, then open the returned asset ID, for example
   `python Engine ctl open asset:Graphics/Characters/X --screenshot --scale 0.5`. Read the image; use `ctl frames` for
   motion. The asset is its own viewport preview: no separate work scene is needed. A work scene is for a model,
   scene or interaction that the asset alone cannot show.
6. **Use:** game code includes `#include "Assets/Graphics/Characters/X.h"` and calls the draw function directly.
7. **Finish:** run `python Engine asset unlisted <files>` with the files you changed. Register every listed drawing
   or explain why it is not registered (for example, it reads a model or the project's agent file chooses another
   location). This heuristic is a reminder, not a failing check; do not migrate unrelated existing code.
