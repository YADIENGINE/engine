# Modules

Oyun gerektirdiği hedefe link verir ve yalnız modülün public `<Modül>.hpp` başlığını kullanır.
Core oyuncu seçenekleri için Persistence/JsonIO kullanır; diğer modüller oyunun seçimine bağlıdır.

| Modül / CMake hedefi | Kullanım |
| --- | --- |
| [JsonIO](JsonIO/README.md) / EngineJsonIO | JSON belge, dosya, atomik yazım, şifreleme, gzip yazımı |
| [Persistence](Persistence/README.md) / EnginePersistence | Kalıcı key/value, key yönetimi, backup ve kurtarma |
| [Localization](Localization/README.md) / EngineLocalization | Dil kataloğu, fallback, placeholder ve sayı biçimleme |
| [ScoreBoard](ScoreBoard/README.md) / EngineScoreBoard | Skor geçmişi, sıralama ve oyuna ait doğrulama |
| [RunTracking](RunTracking/README.md) / EngineRunTracking (+ EngineRunTrackingRuntime) | Başsız gzip JSON koşu arşivi; isteğe bağlı application bağlamı |
| [Profiler](Profiler/README.md) / EngineProfiler (+ EngineProfilerRuntime) | Opt-in CPU/GPU ölçümü, kare istatistikleri ve performans karşılaştırmaları |
| [Version](Version/README.md) / EngineVersion | Kurulu sürüm değişimi ve patch-note sorguları |
| [Steam](Steam/README.md) / EngineSteam | Başlatma/hesap tespiti; mevcut backend sınırları README'de |

```cmake
engine_add_game(MyGame SOURCES Game/CreateGame.cpp Game/MyGame.cpp
    LIBRARIES EngineLocalization EngineVersion)
```

Her README API, bağımlılık, sahiplik ve hataları açıklar. Gelecek işler:
[JsonIO](JsonIO/Roadmap.md), [Persistence](Persistence/Roadmap.md),
[Localization](Localization/Roadmap.md), [ScoreBoard](ScoreBoard/Roadmap.md),
[Version](Version/Roadmap.md), [Steam](Steam/Roadmap.md).
Planlanan bir iş desteklenen özellik sayılmaz; örneğin Steam'in SDK backend'i
olmadan gerçek achievement unlock vaat edilmez.

Modül geliştiriyorsanız [Module rules](../Contributing/ModuleRules.md) ve
[Testing](../Contributing/Testing.md) rehberlerini izleyin.
