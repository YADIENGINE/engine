# Engine documentation

This folder is for developers making games with the C++20 / raylib-based engine. The engine's name and version are in
`EngineManifest.json`; the documentation does not spell out the name. The engine is a separate repo; games use it as an
`Engine/` submodule. Engine code shipped in games is under `Core/`, the editor under `Editor/`, the tools' shared ImGui
layer under `ToolUi/` ([ToolUi](ToolUi.md)), external dependencies under `ThirdParty/`, documentation under `Docs/`, and
build/editor/test tools and the new project template under `Tools/`; commands use `python Engine <command>`
([ToolsContract](ToolsContract.md)), and `python Engine new` creates a project. Game rules, content catalogs, and
project-specific editor helpers stay in the game's own documentation.

## First steps

0. [Project setup](ProjectSetup.md): `python Engine new`, `Project.json`, commands, submodule.
1. [First game](GettingStarted.md): CMake, factory, game, and first scene.
2. [Architecture](Architecture.md) and [runtime](Runtime.md): ownership, lifecycle, time, and scene transitions.
3. [Graphics](Graphics/README.md), [input](Input.md), [audio](Audio.md), and [assets/platforms](AssetsAndPlatforms.md).
4. [Options and saves](OptionsAndSaves.md), [modules](Modules/README.md), and [gameplay helpers](GameplayHelpers.md).
5. [Editor](Editor/README.md): connect the project and preview production drawing.
6. [Release package](Release.md): `python Engine release`, `Release/` layout, and third-party notices. Every change that
   adds an external asset or library to a game also updates the `Game/Distribution/` notice.

## Topic index

| Need | Document |
| --- | --- |
| Window, canvas, layers, and clipping | [Rendering](Graphics/Rendering.md) |
| Fonts, Unicode, measurement, and line layout | [Text](Graphics/Text.md) |
| Themes, widgets, and backgrounds | [Themes and UI](Graphics/ThemesAndUi.md) |
| Particles, cameras, transitions, and overlays | [Effects](Graphics/Effects.md) |
| Values adjusted without building (`*.tuning.json`) | [Tuning](Tuning.md) |
| General JSON run archive | [RunTracking](Modules/RunTracking/README.md) |
| Offline audio generation with Python | [Audio generation](AudioGeneration.md) |
| Release package, license/credits notices | [Release](Release.md) |
| Writing engine/module tests | [Testing](Contributing/Testing.md) |
| Developing a new module | [Module rules](Contributing/ModuleRules.md) |
| Public API / `Internals/` separation | [API boundaries](Contributing/ApiBoundaries.md) |

Module READMEs describe the supported API; `Roadmap.md` files describe future work. The plan to turn the engine into an
executable editor and the architecture decisions are under `Docs/Design/` in the engine repo (excluded from the
launcher's project copy). The editor's [development notes](Editor/Development/Roadmap.md) and
[history](Editor/Development/History.md) are separate from the user guide. Commands in the documentation are for
developers to run; documentation changes do not require a build.