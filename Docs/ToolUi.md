# ToolUi: shared UI layer for tools

`ToolUi/` (CMake `EngineToolUi`) is the shared ImGui layer for engine tools (editor, launcher). It does not ship in
games; game UI is `Core/Graphics/Ui`. Includes are relative to the engine root (`#include "ToolUi/ToolTheme.h"`). The
target is defined in Windows builds with `ENGINE_BUILD_RUNTIME` enabled and builds only when a tool links it.

| File | Contents |
| --- | --- |
| `ToolUiContext` | ImGui context (rlImGui), fonts, theme, scale. `ToolUiOptions { docking, keyboardNavigation }`. Created after the window (Game), destroyed before it; `BeginFrame(scalePercent)` / `EndFrame()` every frame |
| `ToolTheme` | Cabinet theme (`ApplyToolTheme`), central `ToolPalette` colors / `ToolMetrics` dimensions, font roles (`Ui / Medium / Heading / Technical`), font-independent scale (`ToolUiScale()`), semantic colors (`ToolToneColor`), panel surfaces (`ToolSurfaceColor`) |
| `ToolWidgets` | `BeginToolPanel` / `EndToolPanel`, `ToolTooltip`, `ToolClipText` / `ToolClipMiddle`, `ToolSectionHeader`, `ToolEmptyState`, `ToolStatusBadge`, `ToolButton`, `ToolSegment`, `ToolIconButton`, `ToolToggle`, `ToolBeginCombo`, `ContinueToolToolbar`, `ToolSingleLineButton`, `ToolWrappedSelectable` |
| `ToolPixel` | The identity: the 5x7 pixel font of the launcher's icon as `ToolPixelText`, project tiles (`ToolPixelTile`, initial and color from the name), `ToolStateCell`, `ToolActivity` |
| `ToolWindow` | `ToolGameSettings(applicationId, WindowSettings)`: tool window without a settings file, canvas content, audio, or game font |

Decision and rules of the look: [D23](Design/README.md#d23-tool-look-cabinet).

- **Fonts:** IBM Plex Sans Regular / Medium / SemiBold and IBM Plex Mono Regular, embedded by CMake together with their
  OFL license (`ToolFontLicense()`), so tools look the same on every machine. Font Awesome icons merge into each sans
  weight (`ICON_FA_*`, `<extras/IconsFontAwesome6.h>`). Segoe UI Symbol, when installed, is merged into the technical
  font for symbols terminal UIs use and Plex Mono lacks.
- **Typography:** title 28, main heading 18, body / button 14, section 13, metadata 12 logical pixels. Headings use
  SemiBold, buttons Medium, body Regular. Use `PushFont(ToolFont(role), ToolMetrics::...Size)`.
- **Drawing:** ImGui 1.92's dynamic atlas rasterizes text at the requested final size; it does not enlarge a fixed 14 px
  bitmap. It uses horizontal 2× oversampling, fractional advance, ImGui's pixel-aligned glyph origins, and bilinear
  filtering after atlas upload. UI is not rescaled through a canvas/render texture; it draws directly to the window.
  Plex's hhea height is 1300/1000 em; `ExtraSizeScale` normalizes em size; this correction does not enlarge
  control/layout scale. Otherwise a nominal 14 px setting produces about 11 px em.
- **Scale:** user percentage × monitor DPI is applied once to both fonts and base style. At 100% / 96 DPI, body text is
  14 px and controls 30 px; there is no hidden multiplier. `ToolUiScale()` is unaffected by active heading/font size.
- **Colors:** Cabinet; window `#0F0E0D`, panel `#1C1A18`, elevated `#24221F`, inset content `#0A0908`, controls
  `#2B2825`, selected surface `#332C21`, amber accent `#F4AF65`. Text on accent is `#2A1A0A`. Text / secondary / muted /
  disabled text, hover / pressed, and border / separator colors are in `ToolPalette`. Success is `#8FC79A`, warning uses
  accent, error `#E8858F`, information uses secondary text. Geometry and fonts are independent of palette. `ToolSurface`
  roles: `Header`, `Toolbar`, `Workspace`, `Content`, `Utility`, `Status`, `PanelHeader`. Regions are separated by tone,
  not lines: panes and controls have no border, only what floats above them (popups, dialogs) does.
  `BeginToolPanel` applies a surface and scaled padding to the child region; close it with `EndToolPanel` even if clipped.
  Optional `padding` is in logical px (default 12); dense toolbars can use 8. Corner radius is 3 px everywhere. Below
  100% scale, popup borders and separators remain at least 1 physical px. A disabled main action looks like any disabled
  control. Tools derive their state colors from `ToolTone` (editor: `EditorPhaseColor`).
- **Window:** the tool derives from `Game`, uses `ToolGameSettings`, and draws the whole window as ImGui with
  `ToolUiContext` inside `DrawToolOverlay` (after the window's Painter drawing). `ToolUiContext` colors the native title
  bar to match: dark on Windows 10, the exact color on Windows 11.
- **Look at it:** `LauncherUiSmoke` (launcher `Docs/Development.md`) and `EngineEditorUiSmoke
  <folder> [viewport-image.png]` (editor; target of a tree with `ENGINE_BUILD_EDITOR_TESTS`) draw the real panels in a
  hidden window and write PNGs, so a look change can be judged without opening either tool.
- A new shared widget belongs here only if it serves multiple tools; tool-specific widgets stay in that tool's folder
  (editor parameter / Inspector widgets: `Editor/Ui/Widgets`).