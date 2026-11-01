Route game-development tasks to public headers and compiled examples.

Find your task in the table.
Open its recipe when present; otherwise open the listed header.
Read the linked Docs section only when you need the concept.

Paths below start at the engine root; in a game the engine lives at `Engine/`. C++ includes omit `Core/`;
editor and recipe includes start at the engine root. Run tools from the project root as `python Engine <command>`.

## Visual work loop

For a visual or gameplay change, check `python Engine ctl status`. If it succeeds and an editor is open:

1. Create or reuse a work scene for this request in `Editor/Work/` ([recipe](Recipes/WorkScene.md)). Show only the
   subject: no HUD, menus, unrelated objects or randomness unless asked. Expose likely-tweaked values as parameters.
2. Implement the change in production code, then run `python Engine refresh`; keep the editor open.
3. Run `python Engine ctl open <id>`, then `ctl screenshot` / `ctl frames`. Read the PNGs and fix obvious defects
   (broken, clipped, invisible, wrong timing) before reporting.
4. Leave the work scene open and give the developer a short checklist; aesthetic judgment is theirs.
5. When the developer says the task is done, ask: delete the work scene, or move it to a permanent category?

If no editor is open, say so and suggest opening it; do not launch it unless asked.

## Hard invariants

- Draw through Painter: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Use logical canvas coordinates, y down: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Angles are radians, clockwise: `Docs/Graphics/Painter.md#where-it-comes-from`.
- Keep Painter/scopes inside the drawing call; end scopes in reverse order: `Docs/Graphics/Painter.md#scopes`.
- Game code includes no engine Internals or raylib: `Docs/Contributing/ApiBoundaries.md#three-tiers`, `Docs/AssetsAndPlatforms.md#platform-responsibilities`.
- Do not keep a decorative View clock; use paint.Time(): `Docs/Runtime.md#input-and-time-selection`.
- Pass real frame duration through TimeScale; scenes select their time domain: `Docs/Runtime.md#input-and-time-selection`.
- Keep registered objects alive; shut scenes down before members die: `Docs/Runtime.md#lifecycle`.
- Own surfaces in the scene/View, before renderer teardown: `Docs/Graphics/Painter.md#images-and-surfaces`.

## Tasks

| Task | Header(s) | Recipe | Concept doc |
| --- | --- | --- | --- |
| Create a game, factory and GameSettings | `Core/Runtime/Game.h`<br>`Core/Runtime/EntryPoint.h`<br>`Core/Runtime/GameSettings.h` | `Recipes/SceneTransition.md` | `Docs/ProjectSetup.md#new-project` |
| Configure canvas, window and assets | `Core/Runtime/GameSettings.h`<br>`Core/Graphics/Render/WindowSettings.h` | - | `Docs/AssetsAndPlatforms.md#gamesettings-reference` |
| Add a scene or fade between scenes | `Core/Scene/Scene.h`<br>`Core/Fx/TransitionClock.h` | `Recipes/SceneTransition.md` | `Docs/Runtime.md#input-and-time-selection` |
| Add, disable or remove a GameObject | `Core/GameObjects/GameObject.h`<br>`Core/GameObjects/ObjectList.h` | `Recipes/SceneObjects.md` | `Docs/Runtime.md#lifecycle` |
| Draw an animated character or rotating shape | `Core/Graphics/Paint/Painter.h`<br>`Core/Graphics/Anim/SpringPoint.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Painter.md#shapes` |
| Draw a curved outline or polygon | `Core/Graphics/Paint/Path.h`<br>`Core/Graphics/Paint/PaintStyle.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Painter.md#shapes` |
| Clip, transform, fade or blend drawing | `Core/Graphics/Paint/Painter.h`<br>`Core/Utilities/Affine2.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#scopes` |
| Fade a multi-shape sprite as one | `Core/Graphics/Paint/Painter.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#scopes` |
| Cache an offscreen layer | `Core/Graphics/Surface/Surface.h`<br>`Core/Graphics/Paint/Painter.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#images-and-surfaces` |
| Draw an image, nine-slice or warped surface | `Core/Graphics/Surface/TextureRef.h`<br>`Core/Graphics/Paint/Painter.h` | - | `Docs/Graphics/Painter.md#images-and-surfaces` |
| Add bloom or choose rendering quality | `Core/Graphics/Post/PostSettings.h`<br>`Core/Graphics/Post/RenderQuality.h`<br>`Core/Runtime/GraphicsSettings.h` | - | `Docs/Graphics/Painter.md#post-process-and-quality` |
| Load fonts, measure, wrap or align text | `Core/Graphics/Text/FontFace.h`<br>`Core/Graphics/Text/TextStyle.h`<br>`Core/Graphics/Text/TextBlock.h` | `Recipes/TextLayout.md` | `Docs/Graphics/Text.md#drawing-and-layout` |
| Choose colors or work with floating geometry | `Core/Utilities/Color4.h`<br>`Core/Utilities/Float2.h`<br>`Core/Utilities/FloatRect.h`<br>`Core/Utilities/FloatRange.h` | - | `Docs/Graphics/Painter.md#basic-types` |
| Ease a pose or animate a counter | `Core/Graphics/Anim/Easing.h`<br>`Core/Graphics/Anim/SpringPoint.h`<br>`Core/Graphics/Anim/AnimatedCounter.h`<br>`Core/Graphics/Anim/ButtonAnim.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Rendering.md#geometry-and-timing` |
| Convert coordinates or use math helpers | `Core/Utilities/MathUtils.h`<br>`Core/Utilities/UiTransform.h`<br>`Core/Utilities/Vec2.h`<br>`Core/Utilities/Rect.h` | - | `Docs/Graphics/Rendering.md#geometry-and-timing` |
| Draw a shared icon or add a new shape | `Core/Graphics/Icons/Icons.h` | `Recipes/IconShape.md` | `Docs/Graphics/Painter.md#helpers-and-theme` |
| Choose a theme or make a themed button | `Core/Graphics/Style/Theme.h`<br>`Core/Graphics/Style/ThemePalette.h`<br>`Core/Graphics/Style/NeonArcadeTheme.h`<br>`Core/Graphics/Ui/UiDraw.h`<br>`Core/Graphics/Ui/UiButton.h` | `Recipes/ThemedMenu.md` | `Docs/Graphics/ThemesAndUi.md#themes-and-ui-widgets` |
| Draw explicit-style neon shapes and UI | `Core/Graphics/Draw/DrawUtils.h`<br>`Core/Graphics/Draw/Neon.h` | - | `Docs/Graphics/Painter.md#helpers-and-theme` |
| Add menu/play backdrops or a grid | `Core/Graphics/Ui/Backdrop.h`<br>`Core/Graphics/Draw/BackgroundDraw.h` | - | `Docs/Graphics/ThemesAndUi.md#themes-and-ui-widgets` |
| Emit a particle burst, shake and flash | `Core/Fx/FxStage.h`<br>`Core/Fx/FxStyle.h`<br>`Core/Fx/FxQuality.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#effect-stage-and-particle-recipes` |
| Customize particles or trails | `Core/Fx/ParticleSystem2D.h`<br>`Core/Fx/ParticleRecipes.h`<br>`Core/Graphics/Draw/ParticleDraw.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#graphics-and-fx` |
| Frame a cinematic camera or transform world drawing | `Core/Utilities/CameraView.h`<br>`Core/Fx/CameraShot.h`<br>`Core/Fx/CameraEffects2D.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Add pop text, toast, banner or speed lines | `Core/Fx/FloatingText2D.h`<br>`Core/Fx/FxText.h`<br>`Core/Fx/OverlayEffects2D.h`<br>`Core/Fx/SpeedLines.h` | - | `Docs/Graphics/Effects.md#graphics-and-fx` |
| Add rings, shockwaves, glitch or a pulse | `Core/Fx/RingEffects2D.h`<br>`Core/Fx/ShockwaveEffects2D.h`<br>`Core/Fx/GlitchEffects2D.h`<br>`Core/Fx/PulseOverlay2D.h` | - | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Draw an iris transition or shatter geometry | `Core/Fx/IrisTransition.h`<br>`Core/Fx/ShatterPattern.h` | - | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Pause gameplay, add hitstop or slow motion | `Core/Fx/TimeScale.h`<br>`Core/Scene/Scene.h` | - | `Docs/Runtime.md#input-and-time-selection` |
| Read keys and navigate a menu | `Core/Input/Input.h`<br>`Core/Input/InputNavigation.h`<br>`Core/Menus/MenuSelection.h`<br>`Core/Menus/MenuInput.h` | `Recipes/ThemedMenu.md` | `Docs/Input.md#physical-snapshot-and-semantic-queries` |
| Handle pointer collections or dragging | `Core/Input/CollectionPointer.h`<br>`Core/Input/DragPointer.h` | - | `Docs/Input.md#collections-and-drag-and-drop` |
| Navigate a grid, headers or a scrolling viewport | `Core/Menus/GridMenuSelection.h`<br>`Core/Menus/HeaderGridNavigation.h`<br>`Core/Menus/GridViewport.h` | - | `Docs/Input.md#menu-selection-and-viewport` |
| Load/play music and effects | `Core/Audio/AudioSystem.h` | - | `Docs/Audio.md#setting-up-a-catalog` |
| Persist options or game progress | `Core/Options/GameOptions.h`<br>`Core/Modules/Persistence/Persistence.h` | `Recipes/SaveProgress.md` | `Docs/OptionsAndSaves.md#saves` |
| Tune values without rebuilding | `Core/Tuning/Tuning.h`<br>`Core/Tuning/TuningFile.h`<br>`Core/Tuning/TuningLive.h` | `Recipes/TuningValue.md` | `Docs/Tuning.md#setup` |
| Parse, write, gzip or encrypt JSON | `Core/Modules/JsonIO/JsonIO.h` | - | `Docs/Modules/JsonIO/README.md#public-api` |
| Store generic values with backup/encryption | `Core/Modules/Persistence/Persistence.h` | `Recipes/SaveProgress.md` | `Docs/Modules/Persistence/README.md#public-api` |
| Translate text and format numbers | `Core/Modules/Localization/Localization.h` | - | `Docs/Modules/Localization/README.md#public-api` |
| Track application/save versions | `Core/Modules/Version/Version.h` | - | `Docs/Modules/Version/README.md#public-api` |
| Use Steam achievements or stats | `Core/Modules/Steam/Steam.h` | - | `Docs/Modules/Steam/README.md#public-api` |
| Rank scores and store player history | `Core/Modules/ScoreBoard/ScoreBoard.h` | - | `Docs/Modules/ScoreBoard/README.md#public-api` |
| Archive run JSON or attach runtime context | `Core/Modules/RunTracking/RunTracking.h` | - | `Docs/Modules/RunTracking/README.md#public-api` |
| Record performance counters and reports | `Core/Modules/Profiler/Profiler.h`<br>`Core/Utilities/FrameObserver.h`<br>`Core/Runtime/FrameExecutionSettings.h`<br>`Core/Graphics/Render/RenderStats.h` | - | `Docs/Modules/Profiler/README.md#recording` |
| Apply stat modifiers and timed effects | `Core/Gameplay/StatSheet.h`<br>`Core/Gameplay/TimedEffectSet.h` | - | `Docs/GameplayHelpers.md#modifier-and-timed-effect-example` |
| Track tiered goals or delayed pickup credit | `Core/Progress/TieredProgress.h`<br>`Core/Fx/CollectFlights.h` | - | `Docs/GameplayHelpers.md#gameplay-frameworks` |
| Pick weighted values or step settings choices | `Core/Utilities/WeightedPick.h`<br>`Core/Utilities/ChoiceList.h`<br>`Core/Utilities/EnumTable.h` | - | `Docs/GameplayHelpers.md#gameplay-frameworks` |
| Iterate UTF-8 characters | `Core/Utilities/Utf8.h` | - | `Docs/Graphics/Painter.md#text` |
| Open a URL or copy text to clipboard | `Core/Platform/Shell.h` | - | `Docs/AssetsAndPlatforms.md#platform-responsibilities` |
| Display a production View in an editor scene | `Editor/Api/EditorScene.h`<br>`Editor/Api/SceneRegistry.h`<br>`Editor/Api/SceneServices.h`<br>`Editor/Worker/WorkerHost.h` | `Recipes/CharacterScene.md` | `Docs/Editor/GettingStarted.md#cmake-target` |
| Create a focused scene for a task | `Editor/Api/SceneAutoRegistration.h`<br>`Editor/Api/SceneRegistry.h` | `Recipes/WorkScene.md` | `Docs/Editor/Automation.md` |
| Expose scene controls, actions or inspection | `Editor/Api/SceneParameter.h`<br>`Editor/Api/SceneInspection.h` | `Recipes/CharacterScene.md` | `Docs/Editor/GettingStarted.md#editorfirstscenecpp` |
| Look at a scene or game flow: open, set, seek, screenshot, frame grid, inspect; show it to the developer | `Tools/commands/ctl.py` | - | `Docs/Editor/Automation.md#commands` |
| Set up, run, check or test a project | `Tools/cli.py` | - | `Docs/ToolsContract.md#commands` |
| Package a release and third-party notices | `Tools/commands/release.py` | - | `Docs/Release.md#checks` |
| Generate audio assets offline | `Tools/AudioGeneration/generator.py` | - | `Docs/AudioGeneration.md#a-sound-set-in-one-file` |

## API index

Open the matching `Api/` for signatures and declaration comments. Headers are authoritative;
macros are not expanded and conditional branches appear as authored. Regenerate with `python Engine check --fix`.
