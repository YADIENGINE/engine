# Engine Editor

A Windows development tool that displays editor scenes using actual production drawing in a separate worker process. General infrastructure
lives in `Engine/Editor/`; the game catalog and worker adapter live in the consumer project. Normal game and headless
targets do not acquire editor/ImGui dependencies.

For agent-driven visual or gameplay changes, follow the
[visual-change skill](../../AgentDocs/Skills/visual-change/SKILL.md) and
[work-scene recipe](../../AgentDocs/Recipes/WorkScene.md).

## Terms

editor scene (EditorScene): a catalog entry opened in the viewport; not an engine Scene. Game screens are engine
Scenes (Scene). A Game Flow editor scene runs real game Scenes.

Migration tables for API and tools changes are in [cumulative migration notes](Development/Migrations.md).

1. [Connecting a project and the first scene](GettingStarted.md).
2. [UI and playback usage](Usage.md).
3. [Designing scenes](CreatingScenes.md).
4. [Public API and project file](Api.md).
5. [Architecture](Architecture.md) and [checks](Testing.md).
6. [Profiler: live measurement, benchmarks, and before/after comparison](Profiler.md).
7. [Automation: driving the open editor from scripts and agents (`ctl`)](Automation.md).
8. [Future editor UI capabilities and prerequisites](TODO.md).

`ENGINE_BUILD_EDITOR=ON` and `ENGINE_BUILD_RUNTIME=ON` enable Windows editor targets; they are disabled by default.
`ENGINE_BUILD_EDITOR_TESTS` enables independent headless infrastructure tests. Build & Refresh builds only the worker
target in the project recipe. Engine Editor/API changes require rebuilding editor and worker together.

Current limits are in the [development notes](Development/Roadmap.md), and rationale in the
[history](Development/History.md).
