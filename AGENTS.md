If you are using the engine from a game, this file is not for you; read [AgentDocs/Index.md](AgentDocs/Index.md).

# Engine contributors

The engine knows no game: never add game names, content, paths, types, or rules.
Use `MyProject` / `MyGame` in examples. This is a public MIT repository; keep secrets and personal data out.
Read the name and version from `EngineManifest.json`; prose says "engine", commands use `<engine>`.
Code reads CMake's `ENGINE_NAME` / `ENGINE_VERSION`; Python reads the manifest.

## Workflow

Inspect `git status` first; preserve unrelated work. Work on a branch inside this repository.
Build and test changes yourself. Commands run as `python . <command>` here,
or `python Engine <command>` in a consuming project: build, run, release, test, editor, refresh, new, info, check.
Commands, exit codes and manifest compatibility: [Tools contract](Docs/ToolsContract.md#changing-the-contract).
Breaking contract changes increment `toolsContract` and update that document together.
Standalone tests: `python . test [--label REGEX | --filter REGEX]`.
Core tests use label `engine`; modules use labels such as `json-io` and `persistence`.
Reports: `build/tests/reports/<run-id>/results.json` (or the project's configured test directory).
Compiler-free tools: `python -m unittest discover -s Tools/Tests -t .`.
Notices: `python -m unittest discover -s ThirdParty/Tests`; both Python suites have CTest label `engine`.
Run the smallest affected selection; follow [Testing](Docs/Contributing/Testing.md#writing-tests).
Run `python . check` for include boundaries, tuning validity and AgentDocs freshness.
`python . check --fix` formats valid tuning files, regenerates API pages and resyncs recipe snippets.

## Boundaries and dependencies

Use public `.h` facades; put implementation under its owner's `Internals/`, never a root `Internals/`.
Games and recipes never include engine internals. Core internals are engine-private;
module, editor and terminal internals are owner-private: [API boundaries](Docs/Contributing/ApiBoundaries.md#three-tiers).
New modules follow [Module rules](Docs/Contributing/ModuleRules.md#1-directory-structure).
raylib belongs only in platform implementation: window/renderer, input, audio, platform, ToolUi and editor transport.
Public headers and headless code never include raylib. Expose platform actions through raylib-free facades.
Only `Graphics/Render/Internals/Window` opens/closes the window. Drawing uses `Painter`.
Dependency directions: Anim/Render → Utilities; Draw → Render/Anim; Icons → Draw;
Fx → Draw/Paint/Anim; Style → Draw/Fx style types; Ui → Style/Draw.
Draw, Render and Fx take styles and time as parameters; they never read the theme.
Detailed ownership and target layout: [Repository layout](Docs/Contributing/RepositoryLayout.md#source-and-targets).
Reuse existing math, easing, geometry, drawing and UI helpers before adding another.
Vendor external dependencies unmodified under `ThirdParty/`; never use FetchContent.
Shipping dependency additions/updates also update `ThirdParty/Notices.json` and `Notices/*.txt`;
editor/test-only dependencies are excluded: [Third-party rules](ThirdParty/README.md).

## Code and documentation

C++20, `/W4` clean; `.clang-format` (4 spaces, 120 columns); UTF-8 and LF.
No named namespaces; `.cpp` helpers use anonymous namespaces. Each class has its own `.h` / `.cpp`.
Use `#pragma once`, RAII, no raw new/delete or C arrays; `enum class` on one line.
Include paths are relative to `Core/` (editor: engine root); no `../` includes.
Include order: own header → third party → standard library → project headers.
Types/functions/constants: PascalCase; locals: camelCase; private members: _camelCase.
Getters are `[[nodiscard]]` and const where applicable; avoid global names that collide with raylib.
Python tools use only the standard library and resolve project paths through `Project.json`.
Write documentation in English; existing untranslated documents may be edited in place.
Commit messages are short, imperative and English. Manifest version changes need cumulative notes.

## AgentDocs maintenance

Public API change → regenerate the API index with `check --fix`; `check` enforces freshness.
New public system or plausible agent task → add a routing row in `AgentDocs/Index.md`.
New agent workflow → a skill in `AgentDocs/Skills/<name>/SKILL.md` plus its row in `AgentDocs/Agent.md`; keep
`Agent.md` short (it is loaded every session). `check` validates skills and every `python Engine ...` call in AgentDocs.
Recipe API change → keep its code compiling in the engine test build; resync snippets with `check --fix`.
`AgentDocs/` links to concepts in `Docs/`; never duplicate concept explanations.
The project template contains no AI file/settings; developers opt in from their own agent file.
Detailed maintenance: [Documentation ownership](Docs/Contributing/RepositoryLayout.md#documentation-ownership).
