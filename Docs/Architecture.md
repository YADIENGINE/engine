# Engine architecture

Engine oyunun türlerini, içeriğini, çevirilerini, dengesini ve kayıt anahtarlarını bilmez.
Türeyen oyun içerik ve sahne yönlendirmesini sağlar; Engine ana döngü ve servisleri sahiplenir.

## Responsibilities

| Module | Responsibility |
| --- | --- |
| `EngineHeadless` | Object lifecycle, scenes, scaled time, transition timing, grid selection/viewport, text layout, pointer gestures, player options (`GameOptions`) and run JSON storage, geometry, shatter patterns, UI transforms and [gameplay helpers](GameplayHelpers.md) |
| `Engine` | Window/canvas, drawing, fonts, input sampling, audio, effects and iris drawing, asset preparation, application loop |
| `EngineLocalization` | Standalone [Localization module](Modules/Localization/README.md): authored languages, native names, translation fallback, placeholders and number formatting |
| `EngineScoreBoard` | Standalone [ScoreBoard module](Modules/ScoreBoard/README.md): game-defined validation, durable score history, top-N and per-player ranking |
| Derived game | Initial scene, navigation, rules, models, views, content definitions, its own option and save keys |

`GameSettings` configures the window/title, logical canvas, clear color, application identity, font
and glyphs, and an asset manifest. With no font configured, the engine uses raylib's default font.
When the window is larger than the logical canvas, the canvas texture is allocated at the window's pixel size and
drawing keeps logical coordinates through the projection, so shapes and text rasterize at screen resolution and
presentation is a 1:1 copy; smaller windows draw at the logical size and downscale. Pixel-based raylib calls
therefore need the engine wrappers: `BeginClip` / `BeginUiClip` instead of `BeginScissorMode`. `TextRenderer`
rasterizes small text at its on-screen pixel size (on first use) and snaps glyphs to the pixel grid; layout
measurements are independent of the window size.
Set `ENGINE_BUILD_RUNTIME=OFF` to use the headless targets without fetching/linking raylib.

The optional [Engine Editor](Editor/README.md) has independent, default-OFF targets. It uses the same runtime
without adding editor or UI dependencies to Engine/EngineHeadless. Transient applications can set
`GameSettings::persistentOptions=false` to use supplied window settings without loading/writing preferences.
`WindowSettings::visible=false` keeps a graphics context hidden; `alwaysRun=true` keeps it ticking while minimized.

### Graphics layout

Everything a game's look needs lives under `Graphics/`, so a new game draws its own objects only:

| Folder | Contents | Depends on |
| --- | --- | --- |
| `Graphics/Anim` | `Easing`, `SpringPoint`, `AnimatedCounter`, `ButtonAnim` (headless) | `Utilities` |
| `Graphics/Render` | `RenderLayer`, `ShadowMask`, `UiRegion`; `Internals/`: `Window`, `Canvas`, `Renderer`, `RenderTargetStack`, `RenderExecutionContext` | `Utilities` |
| `Graphics/Draw` | `DrawUtils`, `VectorMath`, `Neon`, `TextRenderer`, `TextLayout`, `BackgroundDraw`, `EffectQuality` | `Render`, `Anim` |
| `Graphics/Icons` | Shared icon library: shape-named `Icon` + `DrawIcon` | `Draw` |
| `Graphics/Style` | `Theme`, `NeonArcadeTheme` | `Draw`, `Fx` (style types) |
| `Graphics/Ui` | Themed widgets (`DrawUi*`, `UiButton`, `Backdrop`) | `Style`, `Draw` |

Particle shape drawing is shared by `Fx/ParticleSystem2D` and static previews through
`Graphics/Draw/ParticleDraw`; it accepts explicit pose/style and never reads a theme.

Only `Ui` reads the theme. Shared shapes take explicit colors/styles: `DrawDiamond(Lines)`, `RowSlot` (centre of
item `i` in a centred row of pips/stars/badges), `DrawLightning(from, to, seed, color, alpha, LightningStyle)`,
`DrawNeonRing`, and `DrawProgressBar(bar, ProgressBarLook, ProgressBarStyle)` (themed: `DrawUiProgressBar`). When a
game needs a shape another game could use, it is added here (shape-named, style-parametric) rather than in the game.


## Sahiplik ve bağımlılıklar

- `Runtime/Game`: renderer, font, input, ses, seçenekler ve saat; türeyen oyunun sahneleri değer üyedir.
- `Scene/Internals/SceneManager` (Engine-private): sahnelerin referans kayıtları, etkin/bekleyen hedef, geçiş ve kapanış; headless.
- `Scene/Scene`: kendi nesne listesi, gerçek zaman hook'u ve pause kararı; modeller lifecycle'dan bağımsız kalabilir.
- `GameObjects/`: GameObject ve ObjectList; referans kaydı veya dinamik nesne sahipliğiyle sıralı lifecycle.
- Bağımlılık yönü Runtime → Scene → GameObjects; SceneManager, Scene, GameObject ve ObjectList EngineHeadless'da,
  Game ise raylib'li Engine hedefindedir.
- `Graphics/Anim`, temel geometri ve gameplay yardımcıları headless kullanılabilir.
- `Graphics/Draw` → Render/Anim; Icons → Draw; Fx → Draw/Render/Anim;
  Style → Draw/Fx stil tipleri; Ui → Style/Draw. Temayı yalnız üst seviye Ui okur.
- Engine → Game, modül → başka modülün Internals'ı ve dairesel modül bağımlılığı yasaktır.
- GL kaynağı sahipleri pencere kurulduktan sonra oluşturulur ve pencere kapanmadan yok edilir.
  Kaynak sahibi üye, onu kullanan üyeden önce bildirilir.

`EngineHeadless` seçenek depolamak için Persistence/JsonIO'ya bağımlıdır.
Modüllerin ayrı hedef olması CMake configure sırasında yalnız seçilen modüllerin tanımlandığı
anlamına gelmez; mevcut CMake modül hedeflerini tanımlar, tüketici gerekli hedeflere link verir.

## Convenience includes

Game code that uses several components can include a focused umbrella header:

| Header | Public components |
| --- | --- |
| `Fx/FxImport.h` | FX stage, particle recipes, particles, floating text, rings, camera effects, overlays, speed lines, iris drawing |
| `Graphics/Draw/GraphicsImport.h` | DrawUtils, Easing, Neon, TextRenderer, TextStyle |
| `Runtime/RuntimeImport.h` | Game, GameSettings, GameObject, Scene |

These are explicit include lists, not effect registration or object creation. Add a new public
component's header to the relevant umbrella and its `.cpp` to `Engine/CMakeLists.txt` when needed.
Keep game presets and views outside engine umbrellas. Window/canvas/renderer setup, background drawing,
and pointer gestures retain separate includes to keep these groups focused.

`Graphics/Draw/VectorMath.h` (Vector2 operators from raymath plus `Length`, `Distance`, `Dot`, `Normalized`, `Rotated`,
`Perpendicular`, `Polar`, `ToVector2`) is included explicitly, not through `GraphicsImport`, because raymath adds
global names such as `Clamp` and `Lerp`.

Specific headers remain supported and should be used inside Engine and by headless consumers.
In particular, include `Fx/TimeScale.h`, `Fx/TransitionClock.h`, `Scene/Scene.h`, or
`GameObjects/GameObject.h` directly when using only those components without raylib. Input has explicit headless public
headers (`Input.hpp`, `InputNavigation.hpp`, `DragPointer.hpp`, `CollectionPointer.hpp`); device sampling and gesture
implementation stay private in `Input/Internals/`. See [Input](Input.md). There is no global EngineImport header.


Detaylar: [runtime](Runtime.md), [grafik](Graphics/README.md), [modül seçimi](Modules/README.md).
