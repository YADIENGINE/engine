# ToolUi: shared UI layer for tools

`ToolUi/` (CMake `EngineToolUi`) is the shared ImGui layer for engine tools (editor, launcher). It does not ship in
games; game UI is `Core/Graphics/Ui`. Includes are relative to the engine root (`#include "ToolUi/ToolTheme.h"`). The
target is defined in Windows builds with `ENGINE_BUILD_RUNTIME` enabled and builds only when a tool links it.

| File | Contents |
| --- | --- |
| `ToolUiContext` | ImGui context (rlImGui), fonts, theme, scale. `ToolUiOptions { docking, keyboardNavigation }`. Created after the window (Game), destroyed before it; `BeginFrame(scalePercent)` / `EndFrame()` every frame |
| `ToolTheme` | Arcade Cabinet theme (`ApplyToolTheme`), central `ToolPalette` colors / `ToolMetrics` dimensions, font roles (`Ui / Medium / Heading / Technical`), font-independent scale (`ToolUiScale()`), semantic colors (`ToolToneColor`), panel surfaces (`ToolSurfaceColor`) |
| `ToolWidgets` | `BeginToolPanel` / `EndToolPanel`, `ToolTooltip`, `ToolSectionHeader`, `ToolEmptyState`, `ToolStatusBadge`, `ToolButton`, `ToolSegment`, `ToolIconButton`, `ContinueToolToolbar`, `ToolSingleLineButton`, `ToolWrappedSelectable` |
| `ToolWindow` | `ToolGameSettings(applicationId, WindowSettings)`: tool window without a settings file, canvas content, audio, or game font |

- **Fonts:** Windows-installed Segoe UI Regular / Semibold fonts; font files are not packaged. If unavailable, bundled
  Noto Sans Regular / Medium / SemiBold are used. Font Awesome icons merge into each weight (`ICON_FA_*`,
  `<extras/IconsFontAwesome6.h>`). Technical values use Consolas (bundled ImGui font if the system font is absent). The
  Noto OFL license is available through `ToolFontLicense()`; CMake embeds fallback fonts and license in the executable.
- **Typography:** application identity 16, body / button / section 14, main heading 18, metadata 12 logical pixels.
  Headings and buttons use Segoe UI Semibold, body uses Regular. Use `PushFont(ToolFont(rol), ToolMetrics::...Size)`.
- **Drawing:** ImGui 1.92's dynamic atlas rasterizes text at the requested final size; it does not enlarge a fixed 14 px
  bitmap. It uses horizontal 2× oversampling, fractional advance, ImGui's pixel-aligned glyph origins, and bilinear
  filtering after atlas upload. UI is not rescaled through a canvas/render texture; it draws directly to the window.
  Noto's hhea height is 1362/1000 em, Segoe UI's is 2724/2048 em; `ExtraSizeScale` normalizes em size; this correction
  does not enlarge control/layout scale. Otherwise a nominal 14 px setting produces about 10 px em.
- **Scale:** user percentage × monitor DPI is applied once to both fonts and base style. At 100% / 96 DPI, body text is
  14 px and controls 30 px; there is no hidden multiplier. `ToolUiScale()` is unaffected by active heading/font size.
- **Colors:** Arcade Cabinet; window `#1E1A22`, panel `#2C2633`, inset content `#16131B`, controls `#3D3547`, selected
  surface `#493B50`, amber accent `#F4AF65`. Text on accent is `#302013`. Text / secondary / muted / disabled text,
  hover / pressed, and border / subtle separator colors are in `ToolPalette`. Disabled button text is `#746C7E`; the
  surface's disabled opacity is preserved separately. Success is `#89C9A4`, warning uses accent, error `#DB818D`,
  information uses secondary text. Geometry and fonts are independent of palette. `ToolSurface` roles: `Header`,
  `Toolbar`, `Workspace`, `Content`, `Utility`, `Status`, `PanelHeader`. Main regions are separated by their own opaque
  surface and border; ordinary layout subregions remain transparent. `BeginToolPanel` applies a surface, 1 px border,
  and scaled padding to the child region; close it with `EndToolPanel` even if clipped. Optional `padding` is in logical
  px (default 12); dense toolbars can use 8. Corner radius is 3 px. Below 100% scale, panel, window, popup, and control
  borders remain at least 1 physical px. Tools derive their state colors from `ToolTone` (editor: `EditorPhaseColor`).
- **Window:** the tool derives from `Game`, uses `ToolGameSettings`, and draws the whole window as ImGui with
  `ToolUiContext` inside `DrawToolOverlay` (after the window's Painter drawing).
- A new shared widget belongs here only if it serves multiple tools; tool-specific widgets stay in that tool's folder
  (editor parameter / Inspector widgets: `Editor/Ui/Widgets`).