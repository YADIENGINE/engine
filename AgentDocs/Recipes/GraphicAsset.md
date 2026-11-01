Draw a standalone graphic and list its variants as one set in the Assets panel.

APIs: [GraphicAsset.h](../../Editor/Api/GraphicAsset.h), [Painter.h](../../Core/Graphics/Paint/Painter.h).
Workflow: [graphic-asset skill](../Skills/graphic-asset/SKILL.md).

A new standalone graphic (character, enemy, item, prop, icon or background) draws only from parameters and time,
without reading a game model. Write it as `Game/Assets/Graphics/<Folder>/X.h`, `X.cpp` and `X.asset.cpp`; generate the
skeleton with `python Engine asset new X --folder <Folder>`. Keep a model-reading View in place; extract any stateless
drawing part into Graphics and call it from the View. Variants (color, kind) are `items` in one set, not separate assets.
Existing code need not move. The project's own agent file takes precedence if it specifies another location.

For example, run `python Engine asset new Goblin --folder Enemies`. Replace the generated header and drawing with
the following code, using `#include "Assets/Graphics/Enemies/Goblin.h"` as the implementation's own header include.
The recipe's include below points to its compiled test copy. The drawing fits both variants, including the bobbing
motion, inside `box`; it reads no model or global clock.

<!-- recipe:GraphicAsset-header -->
```cpp
#pragma once
#include "Utilities/FloatRect.h"

class Painter;

enum class GoblinKind { Forest, Ice };

void DrawGoblin(Painter& paint, const FloatRect& box, GoblinKind kind, float time);
```
<!-- /recipe -->

<!-- recipe:GraphicAsset -->
```cpp
#include "AgentDocs/Recipes/Code/GraphicAsset.h"
#include <algorithm>
#include <cmath>
#include "Graphics/Paint/Painter.h"

void DrawGoblin(Painter& paint, const FloatRect& box, GoblinKind kind, float time) {
    const float size = std::min(box.width, box.height);
    if (size <= 0.0f) return;
    const Float2 center = box.Center() + Float2{0.0f, std::sin(time * 2.0f) * size * 0.03f};
    const Color4 skin = kind == GoblinKind::Forest ? Rgb(0x70c060) : Rgb(0x80c8f0);
    paint.Triangle(center + Float2{-size * 0.38f, -size * 0.24f},
                   center + Float2{-size * 0.12f, -size * 0.16f},
                   center + Float2{-size * 0.16f, size * 0.08f}, Fill(skin));
    paint.Triangle(center + Float2{size * 0.38f, -size * 0.24f},
                   center + Float2{size * 0.16f, size * 0.08f},
                   center + Float2{size * 0.12f, -size * 0.16f}, Fill(skin));
    paint.Ellipse(center, {size * 0.24f, size * 0.30f}, Fill(skin));
    paint.Circle(center + Float2{-size * 0.09f, -size * 0.06f}, size * 0.025f, Fill(Rgb(0x102030)));
    paint.Circle(center + Float2{size * 0.09f, -size * 0.06f}, size * 0.025f, Fill(Rgb(0x102030)));
}
```
<!-- /recipe -->

In `Game/Assets/Graphics/Enemies/Goblin.asset.cpp`, keep the registration separate from the drawing:

```cpp
#include "Assets/Graphics/Enemies/Goblin.h"
#include "Editor/Api/GraphicAsset.h"

namespace {
GraphicAsset MakeGoblin() {
    GraphicAsset asset;
    // Item order matches GoblinKind. Each name is also the enum token used by Copy usage.
    asset.items = {{"Forest", "green woodland enemy"}, {"Ice", "blue frozen enemy"}};
    asset.usage = "DrawGoblin(paint, box, GoblinKind::{item}, time);";
    asset.draw = [](Painter& paint, const FloatRect& box, std::size_t item, float time) {
        DrawGoblin(paint, box, static_cast<GoblinKind>(item), time);
    };
    return asset;
}
[[maybe_unused]] const bool Registered = RegisterGraphicAsset(&MakeGoblin);
} // namespace
```

The engine's headless recipe target compiles the header and Draw implementation, not this editor-worker registration.
Registration follows [the API example](../../Docs/Editor/Api.md#graphic-assets). Leave `items` empty for a single
drawing; set `thumbnailTime` if its initial frame is empty or a later pose is more useful.

Run `python Engine refresh`, then `python Engine ctl find Goblin` and
`python Engine ctl open asset:Graphics/Enemies/Goblin#Forest --screenshot --scale 0.5`.
Read the screenshot; use `ctl frames` to check motion. The asset is its own viewport preview, so no separate work scene
is needed unless a model, scene or interaction must be shown. If no editor is open, follow the skill's no-editor path.

Game code includes `"Assets/Graphics/Enemies/Goblin.h"` and calls `DrawGoblin` directly; it never includes the editor API
or looks up the asset by string. Before finishing, run
`python Engine asset unlisted Game/Assets/Graphics/Enemies/Goblin.h Game/Assets/Graphics/Enemies/Goblin.cpp Game/Assets/Graphics/Enemies/Goblin.asset.cpp`
with the files you changed. Register every listed drawing or explain why it is not registered.

Concept: [Asset library](../../Docs/Design/AssetLibrary.md#authoring-rule).
