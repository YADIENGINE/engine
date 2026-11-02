# Prebuilt engine

A project on a released engine does not compile the engine. It links a package the launcher carries, so a new project is
ready in seconds and its `build/` folder holds only its own code. The engine's sources stay in the project's `Engine/`:
the compiler reads the headers, and developers and AI agents read the rest.

## What the package holds

`python . prebuilt --output FOLDER` (in the engine repository) compiles the engine once, in Release with the
`MultiThreadedDLL` runtime:

```text
Lib/                  The libraries a project can link, optional modules included
Editor/EngineEditor.exe
EngineTargets.cmake   The same target names as a source build, as imported libraries
Prebuilt.json         Engine version and compiler version
```

Each engine release publishes it beside its record, so a new engine brings its package without a launcher release. The
launcher downloads it with the engine, carries the one for the engine it ships, and passes the folder as
`ENGINE_PREBUILT` ([Tools contract](ToolsContract.md#environment-variables)). Tools remember it in
`build/.engine-prebuilt.json`, so commands run without the launcher (`python Engine build`) make the same choice.

## What changes in a development build

The Debug configuration keeps its name and compiles the project's code with `/Od /Zi /RTC1`, but links the Release
libraries:

| | Source build | Prebuilt |
| --- | --- | --- |
| Breakpoints and stepping in project code | Yes | Yes |
| Stepping into engine code | Yes | No; engine frames have no debug information |
| Standard library debug checks (`_ITERATOR_DEBUG_LEVEL`) | On | Off |
| Live tuning ([Tuning](Tuning.md)) | On | On |
| Programmer errors throw ([Painter](Graphics/Painter.md)) | On | On |

The last two follow the executable, not the libraries: `ENGINE_TUNING_LIVE` is read where `Tuning<T>()` is compiled, and
each game and worker sets `Utilities/DevelopmentChecks.h` with its own flags.

## Choosing the source build

For a full Debug build, for example to step through engine code:

```json
{ "engine": { "build": "source" } }
```

in `Project.json`. A project also builds from source, and says why in the build output, when:

- `Engine/` is a checkout, not a released copy (no `EngineCopy.json`): developing the engine;
- a file in `Engine/` differs from the copy's record: the libraries would not contain the change;
- no package exists for this engine version, or the compiler is older than the package's (a linker must be at least as
  new as what built its inputs);
- the project sets its own `CMAKE_MSVC_RUNTIME_LIBRARY`;
- it is the test build: tests use Debug engine code and its internals.

The change takes effect at the next command; the build tree reconfigures and rebuilds what it must.
