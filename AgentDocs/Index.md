Route game-development tasks to public headers and compiled examples.

Find your task in the table.
Open its recipe when present; otherwise open the listed header.
Read the linked Docs section only when you need the concept.

Paths below start at the engine root; in a game the engine lives at `Engine/`. C++ includes omit `Core/`;
editor and recipe includes start at the engine root. Run tools from the project root as `python Engine <command>`.
Workflow rules, skills and hard invariants: [Agent.md](Agent.md).

## Tasks

| Task | Header(s) | Recipe | Concept doc |
| --- | --- | --- | --- |
| Create a game, factory and GameSettings | `Core/Runtime/Game.h`<br>`Core/Runtime/EntryPoint.h`<br>`Core/Runtime/GameSettings.h` | `Recipes/SceneTransition.md` | `Docs/ProjectSetup.md#new-project` |
| Configure canvas, window and assets | `Core/Runtime/GameSettings.h`<br>`Core/Graphics/Render/WindowSettings.h` | - | `Docs/AssetsAndPlatforms.md#gamesettings-reference` |
| Add a scene or fade between scenes | `Core/Scene/Scene.h`<br>`Core/Scene/TransitionClock.h` | `Recipes/SceneTransition.md` | `Docs/Runtime.md#input-and-time-selection` |
| Add, disable or remove a GameObject | `Core/GameObjects/GameObject.h`<br>`Core/GameObjects/ObjectList.h` | `Recipes/SceneObjects.md` | `Docs/Runtime.md#lifecycle` |
| Draw an animated character or rotating shape | `Core/Graphics/Paint/Painter.h`<br>`Core/Animation/SpringPoint.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Painter.md#shapes` |
| Draw a curved outline or polygon | `Core/Graphics/Paint/Path.h`<br>`Core/Graphics/Paint/PaintStyle.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Painter.md#shapes` |
| Clip, transform, fade or blend drawing | `Core/Graphics/Paint/Painter.h`<br>`Core/Utilities/Geometry/Affine2.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#scopes` |
| Fade a multi-shape sprite as one | `Core/Graphics/Paint/Painter.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#scopes` |
| Cache an offscreen layer | `Core/Graphics/Surface/Surface.h`<br>`Core/Graphics/Paint/Painter.h` | `Recipes/SpriteLayer.md` | `Docs/Graphics/Painter.md#images-and-surfaces` |
| Draw an image, nine-slice or warped surface | `Core/Graphics/Surface/TextureRef.h`<br>`Core/Graphics/Paint/Painter.h` | - | `Docs/Graphics/Painter.md#images-and-surfaces` |
| Add bloom or choose rendering quality | `Core/Graphics/Post/PostSettings.h`<br>`Core/Graphics/Post/RenderQuality.h`<br>`Core/Runtime/GraphicsSettings.h` | - | `Docs/Graphics/Painter.md#post-process-and-quality` |
| Load fonts, measure, wrap or align text | `Core/Graphics/Text/FontFace.h`<br>`Core/Graphics/Text/TextStyle.h`<br>`Core/Graphics/Text/TextBlock.h` | `Recipes/TextLayout.md` | `Docs/Graphics/Text.md#drawing-and-layout` |
| Choose colors or work with floating geometry | `Core/Utilities/Color4.h`<br>`Core/Utilities/Geometry/Float2.h`<br>`Core/Utilities/Geometry/FloatRect.h`<br>`Core/Utilities/Math/FloatRange.h` | - | `Docs/Graphics/Painter.md#basic-types` |
| Ease a pose or animate a counter | `Core/Animation/Easing.h`<br>`Core/Animation/SpringPoint.h`<br>`Core/Animation/AnimatedCounter.h`<br>`Core/Animation/ButtonAnim.h` | `Recipes/AnimatedCharacter.md` | `Docs/Graphics/Rendering.md#geometry-and-timing` |
| Convert coordinates or use math helpers | `Core/Utilities/Math/Scalar.h`<br>`Core/Utilities/Math/Noise.h`<br>`Core/Utilities/Math/Index.h`<br>`Core/Ui/Layout/UiTransform.h`<br>`Core/Utilities/Geometry/CardinalDirection.h`<br>`Core/Utilities/Geometry/Vec2.h`<br>`Core/Utilities/Geometry/Rect.h` | - | `Docs/Graphics/Rendering.md#geometry-and-timing` |
| Draw a new character, enemy, item or prop and list it in the Assets panel | `Editor/Api/GraphicAsset.h` | `Recipes/GraphicAsset.md` | `Docs/Editor/Api.md#graphic-assets` |
| Draw a shared icon or add a new shape | `Assets/Graphics/DefaultIcons.h` | `Recipes/IconShape.md` | `Docs/Graphics/Painter.md#helpers-and-theme` |
| Show a control hint: a key, WASD or arrow set, a mouse button | `Assets/Graphics/Input/KeyPrompts.h` | - | `Docs/Graphics/Painter.md#helpers-and-theme` |
| Write a menu, a dialog, a pause screen or a HUD; buttons, labels, panels; a two-player menu | `Core/Ui/Kernel/Ui.h`<br>`Core/Ui/Widgets/UiButton.h`<br>`Core/Ui/Widgets/UiLabel.h`<br>`Core/Ui/Widgets/UiPanel.h`<br>`Core/Ui/Widgets/UiLayer.h` | `Recipes/UiMenu.md` | `Docs/Ui.md#writing-a-screen` |
| Add a toggle, a slider or a stepper; settings rows | `Core/Ui/Widgets/UiToggle.h`<br>`Core/Ui/Widgets/UiSlider.h`<br>`Core/Ui/Widgets/UiStepper.h` | `Recipes/UiOptions.md` | `Docs/Ui.md#controls` |
| Add tabs, a tooltip, or a scrolling list or grid | `Core/Ui/Widgets/UiTabs.h`<br>`Core/Ui/Widgets/UiTooltip.h`<br>`Core/Ui/Widgets/UiScroll.h`<br>`Core/Ui/Widgets/UiPanel.h` | `Recipes/UiOptions.md` | `Docs/Ui.md#lists-and-grids` |
| Add a ready options screen: volume, display, effects | `Core/Ui/Kit/UiOptionsScreen.h`<br>`Core/Runtime/Game.h` | `Recipes/UiOptions.md` | `Docs/Ui.md#ready-screens` |
| Give menus sounds: move, confirm, back, deny; one button or one screen that differs | `Core/Ui/Sound/UiSounds.h`<br>`Core/Ui/Kernel/UiTypes.h`<br>`Core/Runtime/GameSettings.h` | `Recipes/UiSounds.md` | `Docs/Ui.md#sounds` |
| Write a widget of the game's own | `Core/Ui/Kernel/UiElement.h`<br>`Core/Ui/Kernel/UiTypes.h` | `Recipes/UiMenu.md` | `Docs/Ui.md#widgets-of-your-own` |
| Change how widgets look | `Core/Ui/Kernel/UiLooks.h`<br>`Core/Ui/Looks/UiButtonLook.h`<br>`Core/Ui/Looks/Neon/NeonLooks.h` | - | `Docs/Ui.md#looks` |
| Choose a theme: colours, backgrounds, effects | `Core/Graphics/Style/Theme.h`<br>`Core/Graphics/Style/ThemePalette.h`<br>`Core/Graphics/Style/PlainTheme.h`<br>`Core/Graphics/Style/NeonArcadeTheme.h` | - | `Docs/Graphics/ThemesAndUi.md#themes` |
| Draw explicit-style neon shapes | `Core/Graphics/Draw/DrawUtils.h`<br>`Core/Graphics/Draw/Neon.h` | - | `Docs/Graphics/Painter.md#helpers-and-theme` |
| Add menu/play backdrops or a grid | `Assets/Graphics/Backgrounds/Backdrops.h`<br>`Core/Graphics/Draw/BackgroundDraw.h` | - | `Docs/Graphics/ThemesAndUi.md#backgrounds` |
| Name what an event looks like and play it (`fx.Play("player_hit")`), also from an animation's moment | `Core/Fx/FxCue.h`<br>`Core/Fx/FxCueSet.h`<br>`Core/Fx/FxStage.h` | `Recipes/FxCues.md` | `Docs/Graphics/Effects.md#cues` |
| Emit a particle burst, shake and flash | `Core/Fx/FxStage.h`<br>`Core/Fx/FxStyle.h`<br>`Core/Fx/FxQuality.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#effect-stage-and-particle-recipes` |
| Customize particles or trails | `Core/Fx/Particles/ParticleSystem.h`<br>`Core/Fx/Particles/ParticleRecipes.h`<br>`Core/Graphics/Draw/ParticleDraw.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#graphics-and-fx` |
| Follow a player with a camera, frame several players, keep it inside the level, split the screen, turn the mouse into a world point | `Core/Camera/Camera2D.h`<br>`Core/Camera/Cameras.h`<br>`Core/Camera/CameraSettings.h`<br>`Core/Camera/Debug/CameraGuides.h` | `Recipes/GameCamera.md` | `Docs/Camera.md` |
| A camera that decides gameplay inside a simulation | `Core/Camera/Camera2D.h` | `Recipes/GameCamera.md` | `Docs/Camera.md#a-camera-in-the-rules` |
| Frame a cinematic camera or transform world drawing | `Core/Camera/CameraView.h`<br>`Core/Fx/Camera/CameraShot.h`<br>`Core/Fx/Camera/CameraEffects.h` | `Recipes/FxBurst.md` | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Add pop text, toast, banner or speed lines | `Core/Fx/World/FloatingTexts.h`<br>`Core/Fx/FxText.h`<br>`Core/Fx/Screen/Announcements.h`<br>`Core/Graphics/Draw/SpeedLines.h` | - | `Docs/Graphics/Effects.md#graphics-and-fx` |
| Animate arbitrary popup drawing or emit numeric feedback | `Core/Modules/Popup/Popup.h`<br>`Core/Modules/NumberPopup/NumberPopup.h` | `Recipes/PopupEffects.md` | `Docs/Modules/Popup/README.md`<br>`Docs/Modules/NumberPopup/README.md` |
| Add rings, shockwaves, a flash, glitch or a pulse | `Core/Fx/Screen/ScreenFlash.h`<br>`Core/Fx/World/Rings.h`<br>`Core/Fx/World/Shockwaves.h`<br>`Core/Fx/Screen/ScreenGlitch.h`<br>`Core/Fx/Screen/PulseOverlay.h` | - | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Draw an iris transition or shatter geometry | `Core/Graphics/Draw/IrisTransition.h`<br>`Core/Utilities/Geometry/ShatterPattern.h`<br>`Core/Fx/World/ShatterSpread.h` | - | `Docs/Graphics/Effects.md#cinematic-camera-and-configurable-overlays` |
| Pause gameplay, add hitstop or slow motion | `Core/Runtime/TimeScale.h`<br>`Core/Scene/Scene.h` | - | `Docs/Runtime.md#input-and-time-selection` |
| Write gameplay in fixed steps with replay | `Core/Simulation/Simulation.h`<br>`Core/Simulation/SimulationRunner.h`<br>`Core/Simulation/StepSmoothed.h`<br>`Core/Simulation/SimulationRegistry.h`<br>`Editor/Api/HostedSimulation.h` | `Recipes/Simulation.md` | `Docs/Simulation.md#writing-a-simulation` |
| Play a sound, an effect or hitstop when the rules say something happened (hit, death, pickup) | `Core/Simulation/SimulationEvents.h`<br>`Core/Simulation/SimulationEventReader.h`<br>`Core/Utilities/Overloaded.h` | `Recipes/Simulation.md` | `Docs/Simulation.md#events` |
| Bind keys to per-player actions | `Core/Input/InputBindings.h`<br>`Core/Simulation/PlayerInput.h` | `Recipes/Simulation.md` | `Docs/Simulation.md#input-and-tapes` |
| Test gameplay without a window (`sim run`), or check a replay (`sim verify`, `sim diverge`) | `Tools/commands/sim.py`<br>`Core/Simulation/Scenario.h`<br>`Core/Simulation/ReplayCheck.h`<br>`Core/Simulation/InputTape.h` | `Recipes/Simulation.md` | `Docs/Simulation.md#scenarios` |
| Test shapes against each other, find what is in an area or along a ray, stop fast bullets, push a shape out of a wall | `Core/Physics/Collision/CollisionWorld.h`<br>`Core/Physics/Collision/Collision.h`<br>`Core/Physics/Collision/Shape.h`<br>`Core/Physics/Collision/CollisionMask.h` | `Recipes/Collision.md` | `Docs/Collision.md#many-colliders` |
| Move a value with a curve: a pop-in, a cursor, a recoil; many at once, or with no field of their own | `Core/Animation/Tween.h`<br>`Core/Animation/TweenSet.h`<br>`Core/Animation/Curve.h` | `Recipes/Tween.md` | `Docs/Animation.md#tweens` |
| Animate a character: clips, fades, poses, moments that decide gameplay | `Core/Animation/Animator.h`<br>`Core/Animation/Clip.h`<br>`Core/Animation/ClipSet.h`<br>`Core/Animation/PoseFields.h` | `Recipes/Animator.md` | `Docs/Animation.md#animators` |
| Move one value through several fixed points | `Core/Animation/Track.h` | - | `Docs/Animation.md#tracks` |
| Time a boss attack, a wave plan or a sequence with named moments | `Core/Animation/Timeline.h`<br>`Core/Simulation/TimelineMoments.h` | `Recipes/Timeline.md` | `Docs/Animation.md#timelines` |
| Show colliders on screen | `Core/Physics/Debug/ColliderDraw.h` | `Recipes/Collision.md` | `Docs/Collision.md#seeing-colliders` |
| Use seeded random numbers | `Core/Utilities/Random/Rng.h` | `Recipes/Simulation.md` | `Docs/Simulation.md#writing-a-simulation` |
| Read keys | `Core/Input/Input.h`<br>`Core/Input/InputNavigation.h` | - | `Docs/Input.md#physical-snapshot-and-semantic-queries` |
| Handle pointer collections or dragging | `Core/Input/CollectionPointer.h`<br>`Core/Input/DragPointer.h` | - | `Docs/Input.md#collections-and-drag-and-drop` |
| Add a game's sound effects: variants, pitch range, cooldown, no overlap; a stored sound key | `Core/Audio/SoundRow.h`<br>`Core/Audio/SoundTables.h`<br>`Core/Audio/SoundPlayer.h`<br>`Core/Audio/SoundKey.h` | `Recipes/SoundTable.md` | `Docs/Audio.md#sound-tables` |
| Load/play music; effects numbered by hand | `Core/Audio/AudioSystem.h` | - | `Docs/Audio.md#setting-up-a-catalog` |
| Persist options or game progress | `Core/Options/GameOptions.h`<br>`Core/Modules/Persistence/Persistence.h` | `Recipes/SaveProgress.md` | `Docs/OptionsAndSaves.md#saves` |
| Tune values without rebuilding | `Core/Tuning/Tuning.h`<br>`Core/Tuning/TuningFile.h`<br>`Core/Tuning/TuningLive.h` | `Recipes/TuningValue.md` | `Docs/Tuning.md#setup` |
| Parse, write, gzip or encrypt JSON | `Core/Modules/JsonIO/JsonIO.h` | - | `Docs/Modules/JsonIO/README.md#public-api` |
| Store generic values with backup/encryption | `Core/Modules/Persistence/Persistence.h` | `Recipes/SaveProgress.md` | `Docs/Modules/Persistence/README.md#public-api` |
| Translate text and format numbers | `Core/Modules/Localization/Localization.h` | - | `Docs/Modules/Localization/README.md#public-api` |
| Track application/save versions | `Core/Modules/Version/Version.h` | - | `Docs/Modules/Version/README.md#public-api` |
| Use Steam achievements or stats | `Core/Modules/Steam/Steam.h` | - | `Docs/Modules/Steam/README.md#public-api` |
| Rank scores and store player history | `Core/Modules/ScoreBoard/ScoreBoard.h` | - | `Docs/Modules/ScoreBoard/README.md#public-api` |
| Archive run JSON or attach runtime context | `Core/Modules/RunTracking/RunTracking.h` | - | `Docs/Modules/RunTracking/README.md#public-api` |
| Record performance counters and reports | `Core/Modules/Profiler/Profiler.h`<br>`Core/Diagnostics/FrameObserver.h`<br>`Core/Runtime/FrameExecutionSettings.h`<br>`Core/Graphics/Render/RenderStats.h` | - | `Docs/Modules/Profiler/README.md#recording` |
| Apply stat modifiers and timed effects | `Core/Gameplay/StatSheet.h`<br>`Core/Gameplay/TimedEffectSet.h` | - | `Docs/GameplayHelpers.md#modifier-and-timed-effect-example` |
| Track tiered goals or delayed pickup credit | `Core/Progress/TieredProgress.h`<br>`Core/Progress/CollectFlights.h` | - | `Docs/GameplayHelpers.md#gameplay-frameworks` |
| Pick weighted values or step settings choices | `Core/Utilities/Random/WeightedPick.h`<br>`Core/Options/ChoiceList.h`<br>`Core/Utilities/EnumTable.h` | - | `Docs/GameplayHelpers.md#gameplay-frameworks` |
| Iterate UTF-8 characters | `Core/Utilities/Utf8.h` | - | `Docs/Graphics/Painter.md#text` |
| Open a URL or copy text to clipboard | `Core/Platform/Shell.h` | - | `Docs/AssetsAndPlatforms.md#platform-responsibilities` |
| Display a production View in an editor scene | `Editor/Api/EditorScene.h`<br>`Editor/Api/SceneRegistry.h`<br>`Editor/Api/SceneServices.h`<br>`Editor/Worker/WorkerHost.h` | `Recipes/CharacterScene.md` | `Docs/Editor/GettingStarted.md#cmake-target` |
| Create a focused scene for a task | `Editor/Api/SceneAutoRegistration.h`<br>`Editor/Api/SceneRegistry.h` | `Recipes/WorkScene.md` | `Docs/Editor/Automation.md` |
| Expose scene controls, actions or inspection | `Editor/Api/SceneParameter.h`<br>`Editor/Api/SceneInspection.h` | `Recipes/CharacterScene.md` | `Docs/Editor/GettingStarted.md#editorfirstscenecpp` |
| Find the scene that shows a subject (`ctl find WORDS`) | `Tools/commands/ctl.py` | - | `Docs/Editor/Automation.md#finding-a-scene` |
| Look at a scene or game flow: open, set, seek, screenshot, frame grid, inspect; show it to the developer | `Tools/commands/ctl.py` | - | `Docs/Editor/Automation.md#commands` |
| Set up, run, check or test a project | `Tools/cli.py` | - | `Docs/ToolsContract.md#commands` |
| Package a release and third-party notices | `Tools/commands/release.py` | - | `Docs/Release.md#checks` |
| Keep a sound in the package that code loads by itself (`release.keepAssets`) | `Tools/commands/release.py` | - | `Docs/Release.md#only-the-sounds-a-game-uses` |
| Use the engine's ready-made menu sounds | `Assets/Audio/Ui` | `Recipes/UiSounds.md` | `Docs/Audio.md#ready-made-sounds` |
| Why an engine source edit has no effect; step into engine code; full Debug | `Tools/prebuilt.py` | - | `Docs/Prebuilt.md#choosing-the-source-build` |
| Generate audio assets offline | `Tools/AudioGeneration/generator.py` | - | `Docs/AudioGeneration.md#a-sound-set-in-one-file` |
| Check a sound without hearing it: length, loudness, silent edges, clipping, waveform image (`audio info`, `audio waveform`) | `Tools/commands/audio.py` | - | `Docs/AudioTools.md#audio-files-and-catalog` |
| Find unused sound files, missing catalog files, or which catalog ID plays a file (`audio usage`, `audio catalog`) | `Tools/commands/audio.py` | - | `Docs/AudioTools.md#audio-files-and-catalog` |
| Play a sound for the developer in the editor's Audio Preview (`ctl audio play FILE`) | `Tools/commands/ctl.py` | - | `Docs/AudioTools.md#ctl-audio-playing-for-the-developer` |
| Load or decode a sound file outside the asset root (tools only) | `Core/Audio/AudioFiles.h` | - | `Docs/Audio.md#tools-and-source-files` |

## API index

Open the matching `Api/` for signatures and declaration comments. Headers are authoritative;
macros are not expanded and conditional branches appear as authored. Regenerate with `python Engine check --fix`.
