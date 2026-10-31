# Engine Editor

A Windows development tool that previews actual production drawing in a separate worker process. General infrastructure
lives in `Engine/Editor/`; the game catalog and worker adapter live in the consumer project. Normal game and headless
targets do not acquire editor/ImGui dependencies.

1. [Connecting a project and the first preview](GettingStarted.md).
2. [UI and playback usage](Usage.md).
3. [Designing previews](CreatingPreviews.md).
4. [Public API and project file](Api.md).
5. [Architecture](Architecture.md) and [checks](Testing.md).
6. [Profiler: live measurement, benchmarks, and before/after comparison](Profiler.md).
7. [Future editor UI capabilities and prerequisites](TODO.md).

`ENGINE_BUILD_EDITOR=ON` and `ENGINE_BUILD_RUNTIME=ON` enable Windows editor targets; they are disabled by default.
`ENGINE_BUILD_EDITOR_TESTS` enables independent headless infrastructure tests. Build & Refresh builds only the worker
target in the project recipe. Engine Editor/API changes require rebuilding editor and worker together.

Current limits are in the [development notes](Development/Roadmap.md), and rationale in the
[history](Development/History.md).