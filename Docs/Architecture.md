# Engine architecture

The engine knows no game types, content, translations, balance, or save keys. The derived game provides content and
scene routing; the engine owns the main loop and services.

## Responsibilities

| Module | Responsibility |
| --- | --- |
| `EngineHeadless` | Object lifecycle, scenes, scaled time, transition timing, grid selection/viewport, text layout, pointer gestures, player options (`GameOptions`) and run JSON storage, geometry, shatter patterns, UI transforms and [gameplay helpers](GameplayHelpers.md) |
| `Engine` | Window/canvas, drawing, fonts, input sampling, audio, effects and iris drawing, asset preparation, application loop |
| `EngineLocalization` | Standalone [Localization module](Modules/Localization/README.md): authored languages, native names, translation fallback, placeholders and number formatting |
| `EngineScoreBoard` | Standalone [ScoreBoard module](Modules/ScoreBoard/README.md): game-defined validation, durable score history, top-N and per-player ranking |
| Derived game | Initial scene, navigation, rules, models, views, content definitions, its own option and save keys |

`GameSettings` configures the window/title, logical canvas, clear color, application identity, font and an asset
manifest. With no font configured, shapes remain available but font-dependent drawing requires a font. When the window
is larger than the logical canvas, the canvas texture is allocated at the window's pixel size and drawing keeps logical
coordinates through the projection, so shapes and text rasterize at screen resolution and presentation is a 1:1 copy;
smaller windows draw at the logical size and downscale. Clipping uses scoped `paint.Clip` in logical coordinates.
`FontFace` rasterizes text at its on-screen size on first use; layout measurements are independent of the window size.
Set `ENGINE_BUILD_RUNTIME=OFF` to use the headless targets without fetching/linking raylib.

The optional [Engine Editor](Editor/README.md) has independent, default-OFF targets. It uses the same runtime without
adding editor or UI dependencies to Engine/EngineHeadless. Transient applications can set
`GameSettings::persistentOptions=false` to use supplied window settings without loading/writing preferences.
`WindowSettings::visible=false` keeps a graphics context hidden; `alwaysRun=true` keeps it ticking while minimized.

### Graphics layout

Everything a game's look needs lives under `Graphics/`, so a new game draws its own objects only:

| Folder | Contents | Depends on |
| --- | --- | --- |
| `Graphics/Anim` | `Easing`, `SpringPoint`, `AnimatedCounter`, `ButtonAnim` (headless) | `Utilities` |
| `Graphics/Render` | `RenderStats`; `Internals/`: `Window`, `Renderer`, `RenderPipeline`, `FrameExecutor`, `RenderResources` | `Utilities` |
| `Graphics/Paint`, `Text`, `Surface`, `Post`, `Gpu` | Drawing API, fonts, images, finishing and GPU backends | `Utilities`, private renderer services |
| `Graphics/Draw` | Painter helpers (`DrawUtils.h`, `Neon.h`, `BackgroundDraw.h`, `ParticleDraw.h`) | `Paint`, `Anim` |
| `Menus` | Menu/grid selection, scrolling and tile layout (`GridViewport`, float bounds and tile areas; integer pointer input) | `Utilities`, `Input` |
| `Graphics/Icons` | Shared icon library: shape-named `Icon` + `DrawIcon` | `Draw` |
| `Graphics/Style` | `Theme`, `NeonArcadeTheme` | `Draw`, `Fx` (style types) |
| `Graphics/Ui` | Themed widgets (`DrawUi*`, `UiButton`, `Backdrop`) | `Style`, `Draw` |

Particle shape drawing is shared by `Fx/ParticleSystem2D` and static previews through `Graphics/Draw/ParticleDraw`; it
accepts explicit pose/style and never reads a theme.

The render migration is complete (`Docs/Design/Rendering.md` → R13). All public headers use the `.h` extension.
`Fx/` is Painter-only and headless. Raylib is limited to platform and native tool integration.

Only `Ui` reads the theme. Shared shapes take explicit colors/styles and `Painter&` as their first parameter:
`DrawDiamond`, `RowSlot` (centre of item `i` in a row), `DrawLightning`, `DrawNeonRing`, and `DrawProgressBar` (themed:
`DrawUiProgressBar`). When a game needs a shape another game could use, it is added here (shape-named, style-parametric)
rather than in the game.


## Ownership and dependencies

- `Runtime/Game`: renderer, font, input, audio, options, and clock; the derived game's scenes are value members.
- `Scene/Internals/SceneManager` (Engine-private): scene reference registrations, active/pending target, transitions,
  and shutdown; headless.
- `Scene/Scene`: its own object list, real-time hook, and pause decision; models may remain independent of lifecycle.
- `GameObjects/`: GameObject and ObjectList; ordered lifecycle with reference registration or dynamic object ownership.
- Dependency direction is Runtime → Scene → GameObjects; SceneManager, Scene, GameObject, and ObjectList are in
  EngineHeadless, while Game is in the raylib-based Engine target.
- `Graphics/Anim`, basic geometry, and gameplay helpers are available headlessly.
- `Graphics/Draw` → Render/Anim; Icons → Draw; Fx → Draw/Paint/Anim (headless); Style → Draw/Fx style types; Ui →
  Style/Draw. Only high-level Ui reads the theme.
- Engine → Game, module → another module's Internals, and circular module dependencies are forbidden.
- GL resource owners are created after the window is set up and destroyed before it closes. A resource-owning member is
  declared before members that use it.

`EngineHeadless` depends on Persistence/JsonIO for option storage. Separate module targets do not mean that only
selected modules are defined during CMake configuration; the current CMake defines module targets, and consumers link
the targets they need.

## Convenience includes

Game code that uses several components can include a focused umbrella header:

| Header | Public components |
| --- | --- |
| `Fx/FxImport.h` | FX stage, particle recipes, particles, floating text, rings, camera effects, overlays, speed lines, iris drawing |
| `Runtime/RuntimeImport.h` | Game, GameSettings, GameObject, Scene |

These are explicit include lists, not effect registration or object creation. Add a new public component's header to the
relevant umbrella and its `.cpp` to `Engine/CMakeLists.txt` when needed. Keep game presets and views outside engine
umbrellas. Window/canvas/renderer setup, background drawing, and pointer gestures retain separate includes to keep these
groups focused.

Include `Utilities/Float2.h` for vector operations (`Length`, `Distance`, `Dot`, `Normalized`, `Rotated`,
`Perpendicular`, `Float2::FromAngle`) and the focused `.h` drawing header for each helper.

Specific headers remain supported and should be used inside Engine and by headless consumers. In particular, include
`Fx/TimeScale.h`, `Fx/TransitionClock.h`, `Scene/Scene.h`, or `GameObjects/GameObject.h` directly when using only those
components without raylib. Input has explicit headless public headers (`Input.h`, `InputNavigation.h`,
`DragPointer.h`, `CollectionPointer.h`); device sampling and gesture implementation stay private in
`Input/Internals/`. See [Input](Input.md). There is no global EngineImport header.


Details: [runtime](Runtime.md), [graphics](Graphics/README.md), [module selection](Modules/README.md).
