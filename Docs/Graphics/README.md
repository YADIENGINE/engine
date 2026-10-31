# Graphics

Draw with [Painter](Painter.md); the other guides describe fonts, themes, effects, and presentation using the same API.

All drawing uses logical canvas coordinates chosen by the game; window size is not canvas size. Draw only within a live
renderer frame. The game's scenes/Views own actual production drawing.

| Topic | Guide |
| --- | --- |
| **Drawing API: shapes, text, surfaces, scopes, post-process** | [Painter](Painter.md) |
| Canvas, surfaces, cameras, and clipping | [Rendering](Rendering.md) |
| Fonts, measurement, and text layout | [Text](Text.md) |
| Themes, widgets, and backgrounds | [Themes and UI](ThemesAndUi.md) |
| Particles, cameras, overlays, and presentation clock | [Effects](Effects.md) |

Lower layers take style and time as parameters; themed widgets are in `Graphics/Ui/`. Icons use the shared, shape-named
`Icon` and `DrawIcon` API in `Graphics/Icons/Icons.h`. The View maps an icon's game meaning, rather than the model.
New shapes draw proportionally to `size`.
