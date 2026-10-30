# Steam

Source/API include root: `Engine/Core/Modules/Steam/`. Documentation and plans live here;
`Internals/` and `Tests/` paths below refer to the source module directory.

Opt-in, headless detection of the Steam client (whether Steam launched the game, which
app id it was launched under, which account is logged in) plus an achievement-forwarding
facade. It needs no Steamworks SDK, never contacts Steam servers, and never fails when
Steam is absent. Games that do not use it have no runtime hooks, configuration fields, or work.

## Public API

Include `Modules/Steam/Steam.hpp` and link `EngineSteam`. `SteamStatus`, `SteamAccount`
and `SteamSource` are public value contracts.

```cpp
const Steam steam;
const auto& status = steam.Status();
if (status.account) {
    // e.g. a per-account save folder: "steam_" + std::to_string(status.account->accountId)
}
```

`Steam()` probes once on the calling thread; `Status()` returns the same value for the
object's lifetime and the reference stays valid while the object lives. Construct a new
`Steam` to re-probe (for example after the player logs in). `Steam` is not copyable.
Construction does not throw for any Steam state; probe failures read as "not found".

| Field | Meaning |
|---|---|
| `clientRunning` | A live Steam client process is registered for the current OS user. |
| `launchedBySteam` | The launch environment carries a valid app id (Steam sets it when it starts the game). |
| `appId` | That app id; `SteamAppId`, else the low 24 bits of `SteamGameId`. Zero and malformed values are ignored. |
| `account` | The account logged in to the running client; empty on the login screen or when the client is not running. |
| `source` | `None` when nothing was found, `Environment` when any field came from the environment/registry. |

`SteamAccount::accountId` is the 32-bit id Steam uses for its own `userdata/<id>` folders.
`SteamId64()` converts it to the 64-bit SteamID of an individual public account.

### Trust

`Environment` values are not verified by Steam. They are fine for choosing a save folder
or a display hint; do not use them for ownership, anti-cheat or entitlement decisions.
`SteamSource::Sdk` is reserved for the planned Steamworks backend and is never reported
today (see `Roadmap.md`).

## Achievements

```cpp
steam.UnlockAchievement("ACH_FIRST_WIN"); // when the game's own achievement completes
steam.Update();                           // once per frame
```

The game designs and tracks its own achievements; the module only forwards unlocks.
Achievements must also be defined with the same API names in the Steamworks panel.

- `AchievementsAvailable()` is true only with a connected Steam API (`SteamSource::Sdk`).
  Today no backend can connect, so it is always false.
- `UnlockAchievement(apiName)` is idempotent. When not connected it does nothing: no
  queue, no IO, no error. Unlocks are not remembered across disconnected calls, so the
  game should resend everything it has unlocked locally at startup; Steam ignores
  repeats. An invalid name throws `std::invalid_argument` even when disconnected, so
  typos surface during development without Steam.
- `IsValidAchievementName(apiName)` is constexpr: 1..128 ASCII letters, digits or `_`.
  Use it in a `static_assert` over a game's name table.
- `Update()` does nothing yet; call it once per frame so the planned backend can run
  Steam callbacks and store pending unlocks without game changes.

Call all members on one thread (the game thread).

## Platforms

- Windows: launch variables plus `HKCU\Software\Valve\Steam\ActiveProcess` (`pid`,
  `ActiveUser`). A stale `pid` left by a crashed client does not count as running.
- Other desktop platforms (Linux, macOS): launch variables only; `clientRunning` and
  `account` stay empty.

## Dependencies

None besides the C++ standard library; on Windows it links `advapi32` for the registry.
It does not use `EngineHeadless`, Persistence or raylib. Core does not depend on this module,
and the target is excluded from the default build unless a consumer or its tests select it.

See `Tests/` for launch-variable parsing, machine-independent status invariants and
disconnected achievement calls.
