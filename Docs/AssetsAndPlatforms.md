# Assets and platforms

## Asset root

On desktop, runtime uses the `Assets/` folder beside the executable; do not rely on the working directory.
`engine_add_game(... ASSETS ".../Game/Assets")` copies this folder after building. `Graphics` and `Prefabs` inside it
are reserved for source code and are not copied ([Asset library](#asset-library)). `Engine` is reserved for the
engine's own files, copied to `Assets/Engine` beside every game ([ready-made sounds](Audio.md#ready-made-sounds)); a
folder of that name under `Game/Assets` stops the configure.

`GameSettings::assets` is the relative path list of files in use. Audio catalog paths are added automatically; list a
custom font path explicitly. Images are not loaded from files: visuals are produced by code ([D1](Design/README.md#d1-assets)),
and textures come from `Surface`.

```cpp
GameSettings settings;
settings.font = "Fonts/MyFont.ttf";
settings.assets = {settings.font};
settings.audio.effects = {{SoundId{0}, "Audio/click.wav"}};
```

Empty, absolute, or `..` manifest paths are rejected. A missing file is a startup error. If `font` is empty, the
built-in default font is used. The engine provides manifest preparation and font/audio loading. Every external
(downloaded, licensed) asset has a notice in `Game/Distribution/` and its license file included in the package:
[Release](Release.md).

## Asset library

The editor's Assets panel browses graphics and audio. A graphic opens in the shared Viewport and Inspector; a sound
opens in Audio Preview without changing the running scene. Panel controls: [Editor usage](Editor/Usage.md#window).

| Folder | Contents |
| --- | --- |
| `Engine/Assets/Graphics` | Read-only ready-made drawings; include from `Assets/`, e.g. `Graphics/DefaultIcons.h` |
| `Engine/Assets/Audio` | Read-only ready-made sounds, addressed as `Engine/Audio/...` |
| `Game/Assets/Graphics` | Project drawing code, grouped into folders freely |
| `Game/Assets/Audio`, `Game/Assets/Fonts` | Runtime files; the panel does not move them |

Every graphic is a set, including a single drawing. Search looks inside sets, and Copy usage substitutes the selected
item's name. Registration in `X.asset.cpp` compiles only into the editor worker; games call the drawing directly and
carry no catalog. Authoring, registration and the unlisted-drawing reminder: [Graphic assets](Editor/Api.md#graphic-assets).

Create a graphic with `python Engine asset new NAME --folder FOLDER`; `asset mkdir FOLDER` creates a folder and
`asset move SOURCE DESTINATION` moves a graphic or folder, updating matching literal includes across the project,
including generated tuning includes. The panel uses the same commands. Renaming files, symbols and callers remains a
code refactor. A move outside the editor requires fixing includes yourself. A set moves as a whole; engine assets
are read-only.

The folder tree comes from disk, while metadata and previews come from the built worker. A new graphic is marked
not built until Build & Refresh. Graphics with a `.tuning.json` can open it in the Inspector. Thumbnails are still
images drawn by a separate short-lived worker after a build; `thumbnailTime` chooses the animation time. Old thumbnails
remain available during a broken build, and animation is watched in the Viewport.

Audio Preview plays source files in the editor process, so new sounds need no build. The worker's audio catalog
identifies which sounds are used; unlisted sounds are dimmed. Command-line metadata, waveform and usage tools:
[Audio tools](AudioTools.md).

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
| `assets`, `audio`, `sounds` | empty manifest / catalog / [sound tables](Audio.md#sound-tables) |
| `audioVolume` | [audio steps and groups](Audio.md) |
| `save` | [SaveSettings](../Core/Modules/Persistence/README.md); supports per-file overrides |
| `theme` | PlainTheme |
| `uiLooks`, `uiSounds` | empty: plain widgets, silent menus; [UI](Ui.md#sounds) |
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
