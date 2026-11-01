# Engine tests

Headless Catch2 tests for game-independent code. Sources stay under `Engine/Core/Tests/`
and `Engine/Core/Modules/<Module>/Tests/`; documentation lives here in the engine repository's `Docs/`.
They never link the consuming game's core or include its source headers.

| Executable | Sources | Links | Tags |
| --- | --- | --- | --- |
| EngineTests | Engine/Core/Tests/*.cpp | EngineHeadless, EngineStorage | engine + subsystem |
| EngineRecipeTests | `Core/Tests/RecipeTests.cpp` + compiled `AgentDocs/Recipes/Code/` archive | EngineRecipes, EngineHeadless, EnginePersistence | engine + recipes |
| Engine&lt;Module&gt;Tests | module Tests/*.cpp | module and declared dependencies | module tag |
| Engine.IncludeBoundaries | `python Engine check` over the source tree ([API boundaries](ApiBoundaries.md)) | none (Python 3.10+) | engine |
| Engine.Tools | Tools/Tests (Python unittest; command-line contract, [ToolsContract](../ToolsContract.md)) | none (Python 3.10+) | engine |
| Engine.ThirdPartyNotices | ThirdParty/Tests (Python unittest; notice pins and licenses) | none (Python 3.10+) | engine |
| AudioGeneration.* | Tools/AudioGeneration/Tests (Python unittest, Node) | none (Python 3.10+, NumPy, SciPy; Node for the player) | audio-generation |

`EngineTestSupport` holds game-free helpers: TempDirectory, Diagnostics and GzipInflate.
Include them as `Tests/Support/...`. The test inflater's
[third-party notices](../Modules/JsonIO/ThirdParty.md) apply.

## Building and running

`ENGINE_BUILD_TESTS` defaults OFF. In the standalone engine repository, from its root
(its own `Project.json` and `CMakePresets.json` select the `tests` preset; results in
`build/tests/reports/<run-id>/results.json`):

```powershell
python . test
python . test --label '^persistence$'
```

Inside a game project the project's `python Engine test --label engine` builds the same suite through the game's
test preset. Raw CMake commands, from a project root that has the engine at `Engine/`:

```powershell
cmake -S Engine -B build/engine-tests -DENGINE_BUILD_TESTS=ON -DENGINE_BUILD_RUNTIME=OFF
cmake --build build/engine-tests --target EngineTestSuite --config Debug
ctest --test-dir build/engine-tests -C Debug --output-on-failure
ctest --test-dir build/engine-tests -C Debug -L '^persistence$' --output-on-failure
```

These commands configure/build; they are for a developer who intends to run tests.
Do not use them merely to verify documentation edits.
Catch2 v3.16.0 is fetched on first configure; tests require neither raylib nor a window/audio device.
Cases are discovered with CTest labels, a 30 s timeout and a fixed Catch2 seed.
Editor tests are separately opt-in: [Editor testing](../Editor/Testing.md).

## Adding tests

- Engine: add a source to `engine_tests(EngineTests ...)` in `Engine/Core/Tests/CMakeLists.txt`.
  Name cases `Engine.<Area>.<Behavior>` and tag `[engine][<area>]`.
- Module: create `Engine/Core/Modules/<Module>/Tests/CMakeLists.txt` containing
  `engine_tests(Engine<Module>Tests SOURCES ... LINKS Engine<Module> ...)`.
  It is automatically discovered. Use module tags, e.g. `[persistence]`, rather than game tags.
- Test modules through their sole public header; see [Module rules](ModuleRules.md).
  Module tests link only that module and its declared dependencies, plus test support.

## Writing tests

Assert observable behavior, boundaries and failure results. Arrange explicit inputs and expected
values; never mirror implementation branches in a test. Use public APIs; private fixture access
may arrange state but must not bypass the behavior being tested. Keep fixture implementation
in test sources; do not add public production APIs for tests.

Do not pin cosmetic hashes, animation curve samples, content counts, or a standard library's RNG sequence.
Check ranges, completion/reset, event ordering and replay within the same toolchain.
Exact expectations are appropriate when rounding or another exact result is itself the contract.
Keep finite random samples small; large distribution checks are separate opt-in work and cannot
require every nonzero-probability outcome to appear.

Use explicit delta steps and fixed seeds; no sleeps, live input, graphics or real player data.
Construct Persistence with an explicit TempDirectory path. Use REQUIRE before accessing optional
results and Catch::Approx for floating point. Diagnostics supplies readable geometry assertions.
Shared algorithms are tested here once; consuming game tests cover their own rules and integration.

Run the smallest affected selection for a behavior change. Rendering, actual input, audio,
pause and platform setup require targeted manual checks by the developer.
