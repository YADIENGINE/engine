# Engine

A C++20 2D game engine for Windows desktop, built on raylib 5.5 for the window, input and audio, with its own
GPU-backed 2D renderer (`Painter`), an ImGui editor with live editor scenes, tuning files and a command-line toolchain.
The engine's name and version live in [`EngineManifest.json`](EngineManifest.json).

**To make games with it, install the launcher:** <https://github.com/YADIENGINE/yadien>. The launcher creates
projects, copies a stable engine version into each one and builds them; it bundles Python, CMake and Ninja and
installs the MSVC compiler for you.

This repository is the engine source. Every project carries its own copy in its `Engine/` folder, so a project keeps
building with the engine version it was made with.

## Without the launcher

Requirements: Visual Studio 2022 Build Tools (C++ workload), CMake 3.24+, Python 3.10+.

```sh
# in an empty project folder, with this repository as Engine/
python Engine new
python Engine build
python Engine run
python Engine editor
```

All commands, options and exit codes: [Docs/ToolsContract.md](Docs/ToolsContract.md). Engine tests: `python . test`
from this folder.

## Documentation

[Docs/README.md](Docs/README.md). The documentation is being translated from Turkish to English; some pages are
still Turkish.

## License

MIT, see [LICENSE](LICENSE). Third-party components and their licenses: [ThirdParty/README.md](ThirdParty/README.md).
Games built with the engine get a generated `ThirdPartyNotices.txt` that already contains the engine's own notice.
