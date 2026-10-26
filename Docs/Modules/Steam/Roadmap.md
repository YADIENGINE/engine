# Steam TODO

Planned work; none of it is supported yet. Consumer-specific integration plans belong to the game's own documentation.

- Steamworks backend: optional `STEAMWORKS_SDK_DIR` CMake variable (SDK stays outside the repo),
  `SteamAPI_Init` in the constructor, `SteamAPI_Shutdown` in a destructor, `Update()` running
  callbacks, and `SteamSource::Sdk` with the verified SteamID and app id. Without the variable the
  current environment backend stays the only one. Steam overlay wants init before the graphics device.
- Achievements on that backend: `UnlockAchievement` queues `ISteamUserStats::SetAchievement`,
  `Update()` calls `StoreStats` once per batch. Unknown names log once instead of failing.
- Optional achievement progress (`IndicateAchievementProgress` / stats) if the game wants Steam progress bars.
- Linux/macOS client detection (`~/.steam/registry.vdf` / `registry.vdf` ActiveProcess).
- `steam_appid.txt` next to the executable for development launches outside Steam.
