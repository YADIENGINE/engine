# Assets and platforms

## Asset root

On desktop, runtime uses the `Assets/` folder beside the executable; do not rely on the working directory.
`engine_add_game(... ASSETS ".../Game/Assets")` copies this folder after building.

`GameSettings::assets` is the relative path list of files in use. Audio catalog paths are added automatically; list a
custom font path explicitly:

```cpp
GameSettings settings;
settings.font = "Fonts/MyFont.ttf";
settings.assets = {settings.font, "Images/player.png"};
settings.audio.effects = {{SoundId{0}, "Audio/click.wav"}};
```

Empty, absolute, or `..` manifest paths are rejected. A missing file is a startup error. If `font` is empty, the
built-in default font is used. The engine provides manifest preparation and font/audio loading; it does not provide a
general texture asset manager. Every external (downloaded, licensed) asset has a notice in `Game/Distribution/` and its
license file included in the package: [Release](Release.md). The View using a texture loads and releases the resource
while the GL context is alive.

## GameSettings reference

| Field | Default / meaning |
| --- | --- |
| `window` | 1280×720, title Game, 60 FPS; minimum 640×360, resizing enabled |
| `window.visible`, `alwaysRun` | true / false; hidden context and running while minimized |
| `window.maximized` | false |
| `canvasWidth/Height` | 1280 / 720 logical coordinates |
| `background`, `letterbox` | opaque black `Color4` |
| `smoothCanvas` | true; linear filtering for canvas presentation |
| `applicationId` | Game; persistent identity for user data |
| `font` | empty means no game font; otherwise an asset path, with glyphs loaded on demand; [font guide](Graphics/Text.md) |
| `assets`, `audio` | empty manifest / catalog |
| `audioVolume` | [audio steps and groups](Audio.md) |
| `save` | [SaveSettings](Modules/Persistence/README.md); supports per-file overrides |
| `theme` | NeonArcadeTheme |
| `persistentOptions` | true; when false, player preferences are neither read nor written |
| `fixedWindow` | false; if the host sets true, options do not change capture/window size |

Source contracts: [GameSettings.h](../Core/Runtime/GameSettings.h),
[WindowSettings.h](../Core/Graphics/Render/WindowSettings.h). Since the window title is a `const char*`, a persistent
string literal is simplest. The supplied initial window settings may change when the engine loads user options.

## Platform responsibilities

Windows desktop games and the Windows editor are separate targets; editor dependencies are not added to normal games.
Headless use disables raylib with `ENGINE_BUILD_RUNTIME=OFF`; active targets retain their own dependencies, such as
JSON/Catch2.

`engine_add_game` produces a desktop executable; the [Release](Release.md) workflow prepares the distribution package.

`SavePath(filename)` produces the platform user-data path under applicationId. Asset preparation, save paths, file
storage, and secure randomness are in `Platform/Internals/` (Engine-private); games access them through `Game` and
Persistence/JsonIO. Changing identity or encryption keys affects access to existing saves. Game packaging choices are
not engine API.