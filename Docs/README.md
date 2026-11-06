# Engine documentation

This folder is for developers making games with the C++20 / raylib-based engine. The engine's name and version are in
`EngineManifest.json`; the documentation does not spell out the name. The engine is a separate repo; games use it as an
`Engine/` submodule. Engine code shipped in games is under `Core/`, the editor under `Editor/`, the tools' shared ImGui
layer under `ToolUi/` ([ToolUi](ToolUi.md)), external dependencies under `ThirdParty/`, general documentation under `Docs/`,
module documentation beside its source under `Core/Modules/`, and build/editor/test tools and the new project template
under `Tools/`; commands use `python Engine <command>`
([ToolsContract](ToolsContract.md)), and `python Engine new` creates a project. Game rules, content catalogs, and
project-specific editor helpers stay in the game's own documentation.

## First steps

The engine is built to be driven by your own AI agent: [Using an AI agent](ProjectSetup.md#using-an-ai-agent) connects
it and [Editor automation](Editor/Automation.md) lists what it can do. Agent material is in `AgentDocs/`; start at
[AgentDocs/Agent.md](../AgentDocs/Agent.md).

0. [Project setup](ProjectSetup.md): `python Engine new`, `Project.json`, commands, submodule.
1. [First game](GettingStarted.md): CMake, factory, game, and first scene.
2. [Architecture](Architecture.md) and [runtime](Runtime.md): ownership, lifecycle, time, and scene transitions.
3. [Graphics](Graphics/README.md), [input](Input.md), [audio](Audio.md), and [assets/platforms](AssetsAndPlatforms.md).
4. [Options and saves](OptionsAndSaves.md), [modules](../Core/Modules/README.md), and [gameplay helpers](GameplayHelpers.md).
5. [Editor](Editor/README.md): connect the project and display production drawing in editor scenes.
6. [Release package](Release.md): `python Engine release`, `Release/` layout, and third-party notices. Every change that
   adds an external asset or library to a game also updates the `Game/Distribution/` notice.

## Topic index

| Need | Document |
| --- | --- |
| A game, worker or native test crashed | [Crash reports](Debugging.md) |
| Window, canvas, layers, and clipping | [Rendering](Graphics/Rendering.md) |
| Fonts, Unicode, measurement, and line layout | [Text](Graphics/Text.md) |
| Menus, dialogs, panels, HUD; widgets of your own | [UI](Ui.md) |
| Themes and backgrounds | [Themes](Graphics/ThemesAndUi.md) |
| Particles, cameras, transitions, and overlays | [Effects](Graphics/Effects.md) |
| A dark world with lights in it | [Lighting](Graphics/Lighting.md) |
| Rain, snow, dust, fog; changing the weather | [Atmosphere](Graphics/Atmosphere.md) |
| Values adjusted without building (`*.tuning.json`) | [Tuning](Tuning.md) |
| Gameplay in fixed steps, replay, scenario tests without a window | [Simulation](Simulation.md) |
| Overlaps, rays, fast bullets, finding what is in an area | [Collision](Collision.md) |
| Levels from a seed: rooms, caves, scattered pickups, loot tables with pity | [Procedural](Procedural.md) |
| Tweens, character clips and fades, curves, boss and wave timing | [Animation](Animation.md) |
| A camera that follows, frames several players, stays in the level, split screen | [Camera](Camera.md) |
| General JSON run archive | [RunTracking](../Core/Modules/RunTracking/README.md) |
| Offline audio generation with Python | [Audio generation](AudioGeneration.md) |
| Release package, license/credits notices | [Release](Release.md) |
| Linking a compiled engine; building it from source | [Prebuilt engine](Prebuilt.md) |
| Writing engine/module tests | [Testing](Contributing/Testing.md) |
| Developing a new module | [Module rules](Contributing/ModuleRules.md) |
| Public API / `Internals/` separation | [API boundaries](Contributing/ApiBoundaries.md) |

Module READMEs describe the supported API; `Roadmap.md` files describe future work. The plan to turn the engine into an
executable editor and the architecture decisions are under `Docs/Design/` in the engine repo (excluded from the
launcher's project copy). The editor's [development notes](Editor/Development/Roadmap.md) and
[history](Editor/Development/History.md) are separate from the user guide. Commands in the documentation are for
developers to run; documentation changes do not require a build.
