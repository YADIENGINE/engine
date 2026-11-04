# Themes

Widgets are in [UI](../Ui.md). A theme is the colours, backgrounds and effects they and the rest of the game share.

## Themes

A game's colours are one `Theme` (`Graphics/Style/Theme.h`): `palette` holds role colors (`background`, `surface`,
`surfaceFrame`, `text`, `textSoft`, `textDim`, `overlay`, `accent`, `warning`), `fx` the effect styles (`FxStyle`) and
`backdrop` the Menu/Play/Gallery backgrounds plus scanline geometry. Game-specific accent colors stay in the game.
`GameSettings::theme` selects it. The default is `PlainTheme` (`Graphics/Style/PlainTheme.h`), quiet on purpose, and
`NeonArcadeTheme` (`Graphics/Style/NeonArcadeTheme.h`) is ready to use. `Game` activates it before the derived game's
members are constructed, and `CurrentTheme()` reads it:

```cpp
GameSettings settings;
settings.theme = NeonArcadeTheme;
settings.uiLooks = NeonLooks();
settings.theme.palette.accent = {255, 120, 40, 255};
```

How widgets are shaped is not in the theme: see [looks](../Ui.md#looks). A look reads the palette, so the accent above
changes both plain and neon buttons. `Draw`, `Render` and `Fx` do not read the theme.

`Graphics/Draw/Neon.h` has the neon shapes with explicit styles, for drawing something by hand; the styles `NeonLooks`
uses are `NeonUiStyles()` (`Ui/Looks/Neon/NeonLooks.h`).

## Graphics settings

`GraphicsSettings` holds the player's choices, set by `Game::SetGraphics` (persisted as engine options) and split in
two. `render` (`RenderQuality`) is applied by the Painter: reduced glow weakens halos and Off disables them,
`DrawScanlines` is skipped when background effects are off. `fx` (`Fx/FxQuality.h`) is the current
`Game::Effects()`; scenes hand it to their Fx with `SetQuality` (`FxStage` forwards it): `ParticleSystem::Emit`
scales counts; `CameraEffects` scales shake, kick and punch and ignores `SetMotion` when motion effects are off;
`ScreenFlash` and `ScreenGlitch` are skipped when flashes are off, and then `PulseOverlay` does not beat and
`SlowMotionOverlay` shows no tint; with motion effects off it shows no trails. There is no global quality state. `GraphicsSettings::renderHeight` caps the upscaled
canvas texture (render resolution).

## Backgrounds

`Graphics/Draw/BackgroundDraw.h` provides `DrawGrid2D`, `DrawScanlines`, `DrawVignette` and `BackdropStyle` (scrolling
grid, vignette, scanline alpha). Each drawing call receives `Painter&` first. `Graphics/Style/Backdrop.h` draws theme
roles (`DrawBackdropGrid(paint, BackdropRole::Menu, area)`) and `DrawScanlineOverlay(paint, area, alpha)`; a non-theme
background passes its own style. Grid/scanline color, spacing and thickness are explicit; nonpositive spacing is invalid
and nonpositive thickness/size disables drawing. Views determine layer order. `GameSettings::smoothCanvas` defaults to
true; `letterbox` defaults to opaque black. Font glyphs rasterize at their on-screen size on demand.

`AnimatedCounter` also lives in EngineHeadless. Configure approach speed, punch duration and snap distance.
`Update(target, dt)` approaches increases, snaps decreases, and decays the increase punch; zero speed snaps immediately.
`Clear()` resets values while keeping tuning. The caller computes the eligible total, including deductions for pickups
still in flight. Pass the desired time domain; there is no global clock or gameplay dependency.
