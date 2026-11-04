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
grid, vignette, scanline alpha). Each drawing call receives `Painter&` first. `Graphics/Backgrounds/Backdrops.h` (under `Assets/`) draws theme
roles (`DrawBackdropGrid(paint, BackdropRole::Menu, area)`) and `DrawScanlineOverlay(paint, area, alpha)`; a non-theme
background passes its own style. Grid/scanline color, spacing and thickness are explicit; nonpositive spacing is invalid
and nonpositive thickness/size disables drawing. Views determine layer order. `GameSettings::smoothCanvas` defaults to
true; `letterbox` defaults to opaque black. Font glyphs rasterize at their on-screen size on demand.

### Ready-made scenes

`Graphics/Backgrounds/` also holds whole-screen scenes that read no theme. One call fills `box` with the scene's own
look, and a look struct overrides only what it names. All are Assets panel sets, stay inside `box` through
`Painter::Clip`, so the current transform must not rotate, and take time as a parameter.

| Header | Items |
| --- | --- |
| `Sky.h` | `SkyKind`: `Day`, `Sunset`, `Night` |
| `Clouds.h`, `Mountains.h`, `Sea.h`, `Skyline.h` | The same `SkyKind`, so that scenery matches the sky behind it |
| `StarField.h` | `Still`, `Drift`, `Warp` |
| `Nebula.h` | `Violet`, `Teal`, `Ember` |
| `SynthwaveHorizon.h` | `Magenta`, `Cyan`, `Ember` |
| `Patterns.h` | `Dots`, `DotWave`, `Hexagons`, `HexPulse` |

```cpp
DrawSkyline(paint, box, SkyKind::Night, time);

// The scenery draws its matching sky unless told otherwise, so layers stack:
DrawSky(paint, box, SkyKind::Day, time);
DrawClouds(paint, box, SkyKind::Day, time, {.sky = false});
DrawMountains(paint, box, SkyKind::Day, time, {.sky = false, .scroll = cameraX});
```

`Mountains` and `Skyline` stand still by default; `scroll` follows a camera, far layers moving slower, and `speed` adds
movement over `time`. `StarField` and `Patterns` turn their own fill off with `background = false`.

Recording one of these scenes takes about 1.2 ms of CPU or less at 1920x1080 in a Release build. The dearest are `Nebula`,
computed per frame, and `Mountains`. For a still one, draw it once into a `Surface` the view owns and redraw it only when
`Painter::Stale` says so; `Nebula.h` shows the few lines.

`AnimatedCounter` also lives in EngineHeadless. Configure approach speed, punch duration and snap distance.
`Update(target, dt)` approaches increases, snaps decreases, and decays the increase punch; zero speed snaps immediately.
`Clear()` resets values while keeping tuning. The caller computes the eligible total, including deductions for pickups
still in flight. Pass the desired time domain; there is no global clock or gameplay dependency.
