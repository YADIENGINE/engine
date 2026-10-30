# Themes and UI

### Themes and UI widgets

A game's visual identity is one `Theme` (`Graphics/Style/Theme.h`): `palette` holds role colors (`background`,
`surface`, `surfaceFrame`, `text`, `textSoft`, `textDim`, `overlay`, `accent`, `warning`) and `ui` holds the
neon frame/text/panel/chip/button/round-button/key-cap styles and button animation settings, `fx` the effect
styles (`FxStyle`) and `backdrop` the Menu/Play/Gallery backgrounds plus scanline geometry. Game-specific
accent colors stay in the game. `GameSettings::theme` selects it (default `NeonArcadeTheme`,
`Graphics/Style/NeonArcadeTheme.h`); `Game` activates it before the derived game's members are constructed, and
`CurrentTheme()` reads it. A new game therefore gets the full look by writing nothing, or adjusts fields:

```cpp
GameSettings settings;
settings.theme.palette.accent = {255, 120, 40, 255};
settings.theme.ui.button.notch = 6.0f;
```

`Graphics/Ui/UiDraw.h` draws themed widgets: `DrawUiFrame`, `DrawUiText`, `DrawUiPanel`, `DrawUiChip`,
`DrawUiButton`, `DrawUiButtonBody` (returns the animated body for custom content), `DrawUiRoundButton`,
`DrawUiKeyCap`. `Graphics/Ui/UiButton` is a View value member that animates with the theme's button settings and
produces a `ButtonLook` via `Look(accent)`. For a one-off variant, copy a style from `CurrentTheme().ui`,
change it and call the explicit-style `Graphics/Draw/Neon.h` function. Dependencies run
`Ui` → `Style` → `Draw` / `Fx`; neither `Draw`, `Render` nor `Fx` reads the theme. New widget kinds add a style field to
`UiStyle`, a value in `NeonArcadeTheme` and a `DrawUi*` function, so existing games keep compiling.

`Graphics/Draw/EffectQuality.h` holds the player's effect intensity, set by `Game::SetGraphics` (persisted as
engine options). Helpers read it themselves so callers never branch: Neon drops outer frame glow and text rings
when glow is reduced and all glow (including `DrawGlow`) when off; `ParticleSystem2D::Emit` scales counts;
`CameraEffects2D` scales shake, kick and punch and ignores `SetMotion` when sprint effects are off; `OverlayEffects2D` flashes and `GlitchEffects2D` are skipped when
flashes are off; `DrawScanlines` is skipped when background effects are off. `Canvas::SetMaxHeight` caps the
upscaled canvas texture (render resolution).

`Graphics/Draw/BackgroundDraw.h` provides separate `DrawGrid2D`, `DrawScanlines` and `DrawVignette` calls, and
`BackdropStyle` (scrolling grid, vignette, scanline alpha) with `DrawBackdropGrid/Vignette/Scanlines(style, ...)`.
`Graphics/Ui/Backdrop.h` draws the theme's roles (`DrawBackdropGrid(BackdropRole::Menu, area, time)`) and
`DrawScanlineOverlay(area, alpha)` for scanlines over a scene; a non-theme background passes its own style.
Grid/scanline color, spacing and thickness are explicit; nonpositive spacing is invalid and nonpositive
thickness/size disables drawing. Views determine layer order. `GameSettings::canvasTextureFilter` and
`fontTextureFilter` default to bilinear; `letterbox` defaults to black. Font filtering applies to loaded
fonts and does not alter raylib's borrowed default font.


`AnimatedCounter` also lives in EngineHeadless. Configure approach speed, punch duration and snap distance. `Update(target,
dt)` approaches increases, snaps decreases, and decays the increase punch; zero speed snaps immediately. `Clear()`
resets values while keeping tuning. The caller computes the eligible total, including deductions for pickups still
in flight. Pass the desired time domain; there is no global clock or gameplay dependency.
