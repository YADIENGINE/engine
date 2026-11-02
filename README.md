# Engine

A C++20 2D game engine for Windows desktop. It uses raylib 5.5 for the window, input and audio, and provides its own
GPU-backed 2D renderer (`Painter`), an ImGui editor with live editor scenes, tuning files and a command-line toolchain.
The engine is currently in early `0.x` development; its name and version live in
[`EngineManifest.json`](EngineManifest.json).

## Get started

The recommended way to create and manage projects is the
[YADIEN launcher](https://github.com/YADIENGINE/yadien). It bundles Python, CMake and Ninja, installs the required
MSVC compiler, and gives every project a stable engine version that only changes when you choose to update it.

[Download the latest launcher release](https://github.com/YADIENGINE/yadien/releases), create a project, then use the
launcher UI or run these commands from the project folder:

```powershell
<engine> build
<engine> run
<engine> editor
```

## Manual setup

Requirements: Git, Visual Studio 2022 Build Tools with the C++ workload, CMake 3.24+ and Python 3.10+.

Place this repository at `Engine/` inside an otherwise empty project folder, then let the engine create the project:

```powershell
git clone https://github.com/YADIENGINE/engine.git Engine
python Engine new
python Engine build
python Engine run
python Engine editor
```

Each project keeps its engine under `Engine/`, so it continues to build with the version selected for that project.
See the [tools contract](Docs/ToolsContract.md) for every command, option and exit code.

## What is included?

- A raylib-backed Windows runtime with input, audio, assets and scene lifecycle support.
- The `Painter` renderer, themed UI, text, animation, effects and reusable gameplay helpers.
- A separate-process editor that displays production drawing through live, controllable editor scenes.
- JSON tuning files and optional modules for saves, localization, run tracking, profiling and more.
- A project-aware command-line toolchain for creating, building, testing, editing and packaging games.

## Documentation

Start with the [engine documentation](Docs/README.md), then continue with the guide that matches your task:

- [Create a project](Docs/ProjectSetup.md)
- [Build the first game](Docs/GettingStarted.md)
- [Understand the architecture](Docs/Architecture.md)
- [Connect the editor](Docs/Editor/README.md)
- [Package a release](Docs/Release.md)

The documentation is being translated from Turkish to English; some pages are still Turkish.

## Contributing

This repository contains the engine source. Read the
[repository layout and contribution rules](Docs/Contributing/RepositoryLayout.md) before changing engine code.
Run the standalone test suite from this folder:

```powershell
python . test
```

Testing conventions and narrower test commands are documented in the
[testing guide](Docs/Contributing/Testing.md).

## License

MIT, see [LICENSE](LICENSE). Third-party components and their licenses are listed in
[ThirdParty/README.md](ThirdParty/README.md). Games built with the engine get a generated `ThirdPartyNotices.txt` that
already contains the engine's own notice.
