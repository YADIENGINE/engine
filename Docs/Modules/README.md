# Modules

The game links the target it needs and uses only the module's public `<Module>.h` header. Core uses Persistence/JsonIO
for player options; other modules are the game's choice.

| Module / CMake target | Usage |
| --- | --- |
| [JsonIO](JsonIO/README.md) / EngineJsonIO | JSON documents, files, atomic writes, encryption, gzip writing |
| [Persistence](Persistence/README.md) / EnginePersistence | Persistent key/value, key management, backup and recovery |
| [Localization](Localization/README.md) / EngineLocalization | Language catalog, fallback, placeholders, and number formatting |
| [ScoreBoard](ScoreBoard/README.md) / EngineScoreBoard | Score history, ranking, and game-defined validation |
| [RunTracking](RunTracking/README.md) / EngineRunTracking (+ EngineRunTrackingRuntime) | Headless gzip JSON run archive; optional application context |
| [Profiler](Profiler/README.md) / EngineProfiler (+ EngineProfilerRuntime) | Opt-in CPU/GPU measurement, frame statistics, and performance comparisons |
| [Version](Version/README.md) / EngineVersion | Installed-version changes and patch-note queries |
| [Steam](Steam/README.md) / EngineSteam | Launch/account detection; current backend limits in README |

```cmake
engine_add_game(MyGame SOURCES Game/CreateGame.cpp Game/MyGame.cpp
    LIBRARIES EngineLocalization EngineVersion)
```

Each README explains API, dependencies, ownership, and errors. Future work: [JsonIO](JsonIO/Roadmap.md),
[Persistence](Persistence/Roadmap.md), [Localization](Localization/Roadmap.md), [ScoreBoard](ScoreBoard/Roadmap.md),
[Version](Version/Roadmap.md), [Steam](Steam/Roadmap.md). Planned work is not a supported feature; for example, real
achievement unlocking is not promised without Steam's SDK backend.

When developing a module, follow [Module rules](../Contributing/ModuleRules.md) and
[Testing](../Contributing/Testing.md).
