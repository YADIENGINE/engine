# Options and saves

## Game options

`Options/GameOptions` is the one player-facing preference file, plaintext `settings.json` in the
user-data directory. Store only values a player may edit by hand without harm; anything that must
not be tampered with belongs in an encrypted game save. Values are integers. `Game` loads it before
`Start()` and applies the engine's own options: effect/music/menu-music volume, `SetFpsLimit` (0 means
unlimited), `SetBorderlessFullscreen` and graphics settings. A game that never changes them
writes no file. The game writes its own keys through `Options().SetInt`; the namespaces of engine
keys (`audio.`, `display.`, `graphics.`) are closed to game keys. Engine saves changes, and only changes,
after every scene switch and after `Stop()`; `SaveOptions()` writes immediately.
`Options().LoadResult()` reports `Missing`, `Loaded`, `Recovered` (from `.bak`) or `Reset` (an
unreadable file was archived and replaced) so the game can warn the player. Outdated
storage formats are removed and reported as Missing, without a reset warning.
Options are read only from this file; old progress-file preferences are not imported.

## Saves

`SavePath("progress.json")` chooses the platform's user-data directory using the configured
`applicationId`. Construct `Persistence` with that path; game data keys and their load/save timing belong
to the derived game. A game without saves never needs `Persistence`. `CreatePersistence` applies `GameSettings.save` with per-file overrides and
fills in the application's shared protected-key path. Each instance owns its generic JSON values/settings;
`SaveSettings.keepBackup` enables verified `.bak` snapshots and automatic recovery in `Load()`.
The stateless [JsonIO module](Modules/JsonIO/README.md)
owns JSON conversion and optional authenticated encryption behind replaceable private adapters.
See [Persistence and key storage](Modules/Persistence/README.md). Keep the same identity and keys when
upgrading an existing game.


## Kullanım ve sınırlar

`Game` türevinde `Options().GetInt/SetInt` ile oyuna ait elle düzenlenebilir seçenekleri tutun.
`audio.`, `display.`, `graphics.` engine'e ayrılmıştır. Ses, FPS, ekran ve grafik değişikliği
için ilgili Game/Audio metotlarını kullanın; doğrudan engine anahtarı yazmayın.
Settings ilerleme kaydı değildir; unlock, skor ve benzeri korunan değerleri ayrı Persistence dosyasında tutun.

`CreatePersistence("progress.json")` GameSettings.save politikasını uygular.
Oluşturmak dosyayı yüklemez: `Load()`, load sonucu işleme, değişiklikler ve `Save()`
zamanlaması oyuna aittir. Tam örnek, hata ve kurtarma davranışı:
[Persistence](Modules/Persistence/README.md). Ham JSON için [JsonIO](Modules/JsonIO/README.md).
`persistentOptions=false` yalnız engine tercihlerini devre dışı bırakır;
oyunun kendi Persistence veya telemetry yazımını otomatik kapatmaz.

## Sürüm takibi

Installed-version tracking is opt-in through the headless [Version module](Modules/Version/README.md).
Link `EngineVersion`, include `Modules/Version/Version.hpp`, and call
`Version::Check(Options(), currentVersion, callback)` in the game's `Start()` after response targets
are ready. The module owns detection and saving; the callback authors the game's response to an
upgrade or downgrade. First launch records silently, and unchanged versions do no work.
`Game` has no version inheritance, hooks, or configuration. Games that do not select the module
never compile its implementation. Version values, formatting, constexpr patch-note validation and
`NotesSince` queries share the same public header; patch-note content and UI belong to the game.


## Steam

Steam detection is opt-in through the headless [Steam module](Modules/Steam/README.md). Link
`EngineSteam`, include `Modules/Steam/Steam.hpp`, and construct a `Steam` value where the game needs
to know whether Steam launched it and which account is logged in. It probes once, never fails when
Steam is absent, and needs no Steamworks SDK. `UnlockAchievement(apiName)` forwards the game's own
achievements and is silently dropped while no Steam API is connected (always, until the planned
Steamworks backend). `Game` has no Steam hooks; achievement design, save-path and Cloud policy
belong to the game. Genel plan: [Steam roadmap](Modules/Steam/Roadmap.md).
