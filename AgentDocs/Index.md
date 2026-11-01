Route game-development tasks to public headers and compiled examples.

Find your task in the table.
Open its recipe when present; otherwise open the listed header.
Read the linked Docs section only when you need the concept.

Paths below start at the engine root; in a game the engine lives at `Engine/`. C++ includes omit `Core/`;
editor and recipe includes start at the engine root. Run tools from the project root as `python Engine <command>`.

## Hard invariants

- Draw through Painter: [source](../Docs/Graphics/Painter.md#where-it-comes-from).
- Use logical canvas coordinates, y down: [source](../Docs/Graphics/Painter.md#where-it-comes-from).
- Angles are radians, clockwise: [source](../Docs/Graphics/Painter.md#where-it-comes-from).
- Keep Painter/scopes inside the drawing call; end scopes in reverse order: [source](../Docs/Graphics/Painter.md#scopes).
- Game code includes no engine Internals or raylib: [source](../Docs/Contributing/ApiBoundaries.md#three-tiers), [platform](../Docs/AssetsAndPlatforms.md#platform-responsibilities).
- Do not keep a decorative View clock; use paint.Time(): [source](../Docs/Runtime.md#input-and-time-selection).
- Pass real frame duration through TimeScale; scenes select their time domain: [source](../Docs/Runtime.md#input-and-time-selection).
- Keep registered objects alive; shut scenes down before members die: [source](../Docs/Runtime.md#lifecycle).
- Own surfaces in the scene/View, before renderer teardown: [source](../Docs/Graphics/Painter.md#images-and-surfaces).

## Tasks

| Task | Header(s) | Recipe | Concept doc |
| --- | --- | --- | --- |
| Create a game, factory and GameSettings | [Core/Runtime/Game.h](../Core/Runtime/Game.h)<br>[Core/Runtime/EntryPoint.h](../Core/Runtime/EntryPoint.h)<br>[Core/Runtime/GameSettings.h](../Core/Runtime/GameSettings.h) | [SceneTransition](Recipes/SceneTransition.md) | [guide](../Docs/ProjectSetup.md#new-project) |
| Configure canvas, window and assets | [Core/Runtime/GameSettings.h](../Core/Runtime/GameSettings.h)<br>[Core/Graphics/Render/WindowSettings.h](../Core/Graphics/Render/WindowSettings.h) | - | [guide](../Docs/AssetsAndPlatforms.md#gamesettings-reference) |
| Add a scene or fade between scenes | [Core/Scene/Scene.h](../Core/Scene/Scene.h)<br>[Core/Fx/TransitionClock.h](../Core/Fx/TransitionClock.h) | [SceneTransition](Recipes/SceneTransition.md) | [guide](../Docs/Runtime.md#input-and-time-selection) |
| Add, disable or remove a GameObject | [Core/GameObjects/GameObject.h](../Core/GameObjects/GameObject.h)<br>[Core/GameObjects/ObjectList.h](../Core/GameObjects/ObjectList.h) | [SceneObjects](Recipes/SceneObjects.md) | [guide](../Docs/Runtime.md#lifecycle) |
| Draw an animated character or rotating shape | [Core/Graphics/Paint/Painter.h](../Core/Graphics/Paint/Painter.h)<br>[Core/Graphics/Anim/SpringPoint.h](../Core/Graphics/Anim/SpringPoint.h) | [AnimatedCharacter](Recipes/AnimatedCharacter.md) | [guide](../Docs/Graphics/Painter.md#shapes) |
| Draw a curved outline or polygon | [Core/Graphics/Paint/Path.h](../Core/Graphics/Paint/Path.h)<br>[Core/Graphics/Paint/PaintStyle.h](../Core/Graphics/Paint/PaintStyle.h) | [AnimatedCharacter](Recipes/AnimatedCharacter.md) | [guide](../Docs/Graphics/Painter.md#shapes) |
| Clip, transform, fade or blend drawing | [Core/Graphics/Paint/Painter.h](../Core/Graphics/Paint/Painter.h)<br>[Core/Utilities/Affine2.h](../Core/Utilities/Affine2.h) | [SpriteLayer](Recipes/SpriteLayer.md) | [guide](../Docs/Graphics/Painter.md#scopes) |
| Fade a multi-shape sprite as one | [Core/Graphics/Paint/Painter.h](../Core/Graphics/Paint/Painter.h) | [SpriteLayer](Recipes/SpriteLayer.md) | [guide](../Docs/Graphics/Painter.md#scopes) |
| Cache an offscreen layer | [Core/Graphics/Surface/Surface.h](../Core/Graphics/Surface/Surface.h)<br>[Core/Graphics/Paint/Painter.h](../Core/Graphics/Paint/Painter.h) | [SpriteLayer](Recipes/SpriteLayer.md) | [guide](../Docs/Graphics/Painter.md#images-and-surfaces) |
| Draw an image, nine-slice or warped surface | [Core/Graphics/Surface/TextureRef.h](../Core/Graphics/Surface/TextureRef.h)<br>[Core/Graphics/Paint/Painter.h](../Core/Graphics/Paint/Painter.h) | - | [guide](../Docs/Graphics/Painter.md#images-and-surfaces) |
| Add bloom or choose rendering quality | [Core/Graphics/Post/PostSettings.h](../Core/Graphics/Post/PostSettings.h)<br>[Core/Graphics/Post/RenderQuality.h](../Core/Graphics/Post/RenderQuality.h)<br>[Core/Runtime/GraphicsSettings.h](../Core/Runtime/GraphicsSettings.h) | - | [guide](../Docs/Graphics/Painter.md#post-process-and-quality) |
| Load fonts, measure, wrap or align text | [Core/Graphics/Text/FontFace.h](../Core/Graphics/Text/FontFace.h)<br>[Core/Graphics/Text/TextStyle.h](../Core/Graphics/Text/TextStyle.h)<br>[Core/Graphics/Text/TextBlock.h](../Core/Graphics/Text/TextBlock.h) | [TextLayout](Recipes/TextLayout.md) | [guide](../Docs/Graphics/Text.md#drawing-and-layout) |
| Choose colors or work with floating geometry | [Core/Utilities/Color4.h](../Core/Utilities/Color4.h)<br>[Core/Utilities/Float2.h](../Core/Utilities/Float2.h)<br>[Core/Utilities/FloatRect.h](../Core/Utilities/FloatRect.h)<br>[Core/Utilities/FloatRange.h](../Core/Utilities/FloatRange.h) | - | [guide](../Docs/Graphics/Painter.md#basic-types) |
| Ease a pose or animate a counter | [Core/Graphics/Anim/Easing.h](../Core/Graphics/Anim/Easing.h)<br>[Core/Graphics/Anim/SpringPoint.h](../Core/Graphics/Anim/SpringPoint.h)<br>[Core/Graphics/Anim/AnimatedCounter.h](../Core/Graphics/Anim/AnimatedCounter.h)<br>[Core/Graphics/Anim/ButtonAnim.h](../Core/Graphics/Anim/ButtonAnim.h) | [AnimatedCharacter](Recipes/AnimatedCharacter.md) | [guide](../Docs/Graphics/Rendering.md#geometry-and-timing) |
| Convert coordinates or use math helpers | [Core/Utilities/MathUtils.h](../Core/Utilities/MathUtils.h)<br>[Core/Utilities/UiTransform.h](../Core/Utilities/UiTransform.h)<br>[Core/Utilities/Vec2.h](../Core/Utilities/Vec2.h)<br>[Core/Utilities/Rect.h](../Core/Utilities/Rect.h) | - | [guide](../Docs/Graphics/Rendering.md#geometry-and-timing) |
| Draw a shared icon or add a new shape | [Core/Graphics/Icons/Icons.h](../Core/Graphics/Icons/Icons.h) | [IconShape](Recipes/IconShape.md) | [guide](../Docs/Graphics/Painter.md#helpers-and-theme) |
| Choose a theme or make a themed button | [Core/Graphics/Style/Theme.h](../Core/Graphics/Style/Theme.h)<br>[Core/Graphics/Style/ThemePalette.h](../Core/Graphics/Style/ThemePalette.h)<br>[Core/Graphics/Style/NeonArcadeTheme.h](../Core/Graphics/Style/NeonArcadeTheme.h)<br>[Core/Graphics/Ui/UiDraw.h](../Core/Graphics/Ui/UiDraw.h)<br>[Core/Graphics/Ui/UiButton.h](../Core/Graphics/Ui/UiButton.h) | [ThemedMenu](Recipes/ThemedMenu.md) | [guide](../Docs/Graphics/ThemesAndUi.md#themes-and-ui-widgets) |
| Draw explicit-style neon shapes and UI | [Core/Graphics/Draw/DrawUtils.h](../Core/Graphics/Draw/DrawUtils.h)<br>[Core/Graphics/Draw/Neon.h](../Core/Graphics/Draw/Neon.h) | - | [guide](../Docs/Graphics/Painter.md#helpers-and-theme) |
| Add menu/play backdrops or a grid | [Core/Graphics/Ui/Backdrop.h](../Core/Graphics/Ui/Backdrop.h)<br>[Core/Graphics/Draw/BackgroundDraw.h](../Core/Graphics/Draw/BackgroundDraw.h) | - | [guide](../Docs/Graphics/ThemesAndUi.md#themes-and-ui-widgets) |
| Emit a particle burst, shake and flash | [Core/Fx/FxStage.h](../Core/Fx/FxStage.h)<br>[Core/Fx/FxStyle.h](../Core/Fx/FxStyle.h)<br>[Core/Fx/FxQuality.h](../Core/Fx/FxQuality.h) | [FxBurst](Recipes/FxBurst.md) | [guide](../Docs/Graphics/Effects.md#effect-stage-and-particle-recipes) |
| Customize particles or trails | [Core/Fx/ParticleSystem2D.h](../Core/Fx/ParticleSystem2D.h)<br>[Core/Fx/ParticleRecipes.h](../Core/Fx/ParticleRecipes.h)<br>[Core/Graphics/Draw/ParticleDraw.h](../Core/Graphics/Draw/ParticleDraw.h) | [FxBurst](Recipes/FxBurst.md) | [guide](../Docs/Graphics/Effects.md#graphics-and-fx) |
| Frame a cinematic camera or transform world drawing | [Core/Utilities/CameraView.h](../Core/Utilities/CameraView.h)<br>[Core/Fx/CameraShot.h](../Core/Fx/CameraShot.h)<br>[Core/Fx/CameraEffects2D.h](../Core/Fx/CameraEffects2D.h) | [FxBurst](Recipes/FxBurst.md) | [guide](../Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays) |
| Add pop text, toast, banner or speed lines | [Core/Fx/FloatingText2D.h](../Core/Fx/FloatingText2D.h)<br>[Core/Fx/FxText.h](../Core/Fx/FxText.h)<br>[Core/Fx/OverlayEffects2D.h](../Core/Fx/OverlayEffects2D.h)<br>[Core/Fx/SpeedLines.h](../Core/Fx/SpeedLines.h) | - | [guide](../Docs/Graphics/Effects.md#graphics-and-fx) |
| Add rings, shockwaves, glitch or a pulse | [Core/Fx/RingEffects2D.h](../Core/Fx/RingEffects2D.h)<br>[Core/Fx/ShockwaveEffects2D.h](../Core/Fx/ShockwaveEffects2D.h)<br>[Core/Fx/GlitchEffects2D.h](../Core/Fx/GlitchEffects2D.h)<br>[Core/Fx/PulseOverlay2D.h](../Core/Fx/PulseOverlay2D.h) | - | [guide](../Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays) |
| Draw an iris transition or shatter geometry | [Core/Fx/IrisTransition.h](../Core/Fx/IrisTransition.h)<br>[Core/Fx/ShatterPattern.h](../Core/Fx/ShatterPattern.h) | - | [guide](../Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays) |
| Pause gameplay, add hitstop or slow motion | [Core/Fx/TimeScale.h](../Core/Fx/TimeScale.h)<br>[Core/Scene/Scene.h](../Core/Scene/Scene.h) | - | [guide](../Docs/Runtime.md#input-and-time-selection) |
| Read keys and navigate a menu | [Core/Input/Input.h](../Core/Input/Input.h)<br>[Core/Input/InputNavigation.h](../Core/Input/InputNavigation.h)<br>[Core/Menus/MenuSelection.h](../Core/Menus/MenuSelection.h)<br>[Core/Menus/MenuInput.h](../Core/Menus/MenuInput.h) | [ThemedMenu](Recipes/ThemedMenu.md) | [guide](../Docs/Input.md#physical-snapshot-and-semantic-queries) |
| Handle pointer collections or dragging | [Core/Input/CollectionPointer.h](../Core/Input/CollectionPointer.h)<br>[Core/Input/DragPointer.h](../Core/Input/DragPointer.h) | - | [guide](../Docs/Input.md#collections-and-drag-and-drop) |
| Navigate a grid, headers or a scrolling viewport | [Core/Menus/GridMenuSelection.h](../Core/Menus/GridMenuSelection.h)<br>[Core/Menus/HeaderGridNavigation.h](../Core/Menus/HeaderGridNavigation.h)<br>[Core/Menus/GridViewport.h](../Core/Menus/GridViewport.h) | - | [guide](../Docs/Input.md#menu-selection-and-viewport) |
| Load/play music and effects | [Core/Audio/AudioSystem.h](../Core/Audio/AudioSystem.h) | - | [guide](../Docs/Audio.md#setting-up-a-catalog) |
| Persist options or game progress | [Core/Options/GameOptions.h](../Core/Options/GameOptions.h)<br>[Core/Modules/Persistence/Persistence.h](../Core/Modules/Persistence/Persistence.h) | [SaveProgress](Recipes/SaveProgress.md) | [guide](../Docs/OptionsAndSaves.md#saves) |
| Tune values without rebuilding | [Core/Tuning/Tuning.h](../Core/Tuning/Tuning.h)<br>[Core/Tuning/TuningFile.h](../Core/Tuning/TuningFile.h)<br>[Core/Tuning/TuningLive.h](../Core/Tuning/TuningLive.h) | [TuningValue](Recipes/TuningValue.md) | [guide](../Docs/Tuning.md#setup) |
| Parse, write, gzip or encrypt JSON | [Core/Modules/JsonIO/JsonIO.h](../Core/Modules/JsonIO/JsonIO.h) | - | [guide](../Docs/Modules/JsonIO/README.md#public-api) |
| Store generic values with backup/encryption | [Core/Modules/Persistence/Persistence.h](../Core/Modules/Persistence/Persistence.h) | [SaveProgress](Recipes/SaveProgress.md) | [guide](../Docs/Modules/Persistence/README.md#public-api) |
| Translate text and format numbers | [Core/Modules/Localization/Localization.h](../Core/Modules/Localization/Localization.h) | - | [guide](../Docs/Modules/Localization/README.md#public-api) |
| Track application/save versions | [Core/Modules/Version/Version.h](../Core/Modules/Version/Version.h) | - | [guide](../Docs/Modules/Version/README.md#public-api) |
| Use Steam achievements or stats | [Core/Modules/Steam/Steam.h](../Core/Modules/Steam/Steam.h) | - | [guide](../Docs/Modules/Steam/README.md#public-api) |
| Rank scores and store player history | [Core/Modules/ScoreBoard/ScoreBoard.h](../Core/Modules/ScoreBoard/ScoreBoard.h) | - | [guide](../Docs/Modules/ScoreBoard/README.md#public-api) |
| Archive run JSON or attach runtime context | [Core/Modules/RunTracking/RunTracking.h](../Core/Modules/RunTracking/RunTracking.h) | - | [guide](../Docs/Modules/RunTracking/README.md#public-api) |
| Record performance counters and reports | [Core/Modules/Profiler/Profiler.h](../Core/Modules/Profiler/Profiler.h)<br>[Core/Utilities/FrameObserver.h](../Core/Utilities/FrameObserver.h)<br>[Core/Runtime/FrameExecutionSettings.h](../Core/Runtime/FrameExecutionSettings.h)<br>[Core/Graphics/Render/RenderStats.h](../Core/Graphics/Render/RenderStats.h) | - | [guide](../Docs/Modules/Profiler/README.md#recording) |
| Apply stat modifiers and timed effects | [Core/Gameplay/StatSheet.h](../Core/Gameplay/StatSheet.h)<br>[Core/Gameplay/TimedEffectSet.h](../Core/Gameplay/TimedEffectSet.h) | - | [guide](../Docs/GameplayHelpers.md#modifier-and-timed-effect-example) |
| Track tiered goals or delayed pickup credit | [Core/Progress/TieredProgress.h](../Core/Progress/TieredProgress.h)<br>[Core/Fx/CollectFlights.h](../Core/Fx/CollectFlights.h) | - | [guide](../Docs/GameplayHelpers.md#gameplay-frameworks) |
| Pick weighted values or step settings choices | [Core/Utilities/WeightedPick.h](../Core/Utilities/WeightedPick.h)<br>[Core/Utilities/ChoiceList.h](../Core/Utilities/ChoiceList.h)<br>[Core/Utilities/EnumTable.h](../Core/Utilities/EnumTable.h) | - | [guide](../Docs/GameplayHelpers.md#gameplay-frameworks) |
| Iterate UTF-8 characters | [Core/Utilities/Utf8.h](../Core/Utilities/Utf8.h) | - | [guide](../Docs/Graphics/Painter.md#text) |
| Open a URL or copy text to clipboard | [Core/Platform/Shell.h](../Core/Platform/Shell.h) | - | [guide](../Docs/AssetsAndPlatforms.md#platform-responsibilities) |
| Preview a production View in the editor | [Editor/Api/EditorPreview.h](../Editor/Api/EditorPreview.h)<br>[Editor/Api/PreviewRegistry.h](../Editor/Api/PreviewRegistry.h)<br>[Editor/Api/PreviewServices.h](../Editor/Api/PreviewServices.h)<br>[Editor/Preview/PreviewHost.h](../Editor/Preview/PreviewHost.h) | [CharacterPreview](Recipes/CharacterPreview.md) | [guide](../Docs/Editor/GettingStarted.md#cmake-target) |
| Expose preview controls, actions or inspection | [Editor/Api/PreviewParameter.h](../Editor/Api/PreviewParameter.h)<br>[Editor/Api/PreviewInspection.h](../Editor/Api/PreviewInspection.h) | [CharacterPreview](Recipes/CharacterPreview.md) | [guide](../Docs/Editor/GettingStarted.md#editorfirstpreviewcpp) |
| Set up, run, check or test a project | [Tools/cli.py](../Tools/cli.py) | - | [guide](../Docs/ToolsContract.md#commands) |
| Package a release and third-party notices | [Tools/commands/release.py](../Tools/commands/release.py) | - | [guide](../Docs/Release.md#checks) |
| Generate audio assets offline | [Tools/AudioGeneration/generator.py](../Tools/AudioGeneration/generator.py) | - | [guide](../Docs/AudioGeneration.md#a-sound-set-in-one-file) |

## API index

Open the matching [generated area page](Api/) for signatures and declaration comments. Headers are authoritative;
macros are not expanded and conditional branches appear as authored. Regenerate with `python Engine check --fix`.
