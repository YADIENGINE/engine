# Project settings

A project's title, window, canvas, font and identity live in one file, `Game/Game.settings.json`, which holds only
the values that differ from the engine's defaults. Edit it in the editor under **Project > Project settings**, by hand
or with an agent; the file is the only place these values are kept ([D51](Design/README.md#d51-project-settings)).

```json
{
  "title": "My game",
  "applicationId": "MyGame",
  "version": "1.2.0",
  "icon": "Game/Distribution/Icon.ico",
  "canvasWidth": 640,
  "canvasHeight": 360,
  "artPixel": 3,
  "font": "Fonts/MyFont.ttf"
}
```

`python Engine new` writes the title and the application id. An absent file means every default.

## Settings

| Name | Default | Meaning |
| --- | --- | --- |
| `title` | Game | Window title; the product name in the exe's details |
| `applicationId` | Game | Folder for options and saves in the player's data folder |
| `company` | empty | Company name in the exe's details |
| `version` | 1.0.0 | One to four numbers up to 65535; the exe's file and product version |
| `icon` | empty | A Windows `.ico` from the project folder: window, taskbar and the exe in Explorer |
| `windowWidth`, `windowHeight` | 1280, 720 | Window size when the game starts |
| `windowMinWidth`, `windowMinHeight` | 640, 360 | Smallest size the player can drag the window to |
| `resizable`, `maximized` | true, false | |
| `targetFps` | 60 | Frame rate limit until the player chooses one; 0 is unlimited |
| `canvasWidth`, `canvasHeight` | 1280, 720 | The logical size the game draws at ([Rendering](Graphics/Rendering.md)) |
| `background`, `letterbox` | black | Canvas colour, and the colour around it when the window's shape differs |
| `artPixel` | 0 | Canvas units per art pixel: the world is drawn in pixels this big; 0 is a smooth project. New projects use 3 ([Pixel art](Graphics/PixelArt.md#the-pixel-world)) |
| `pixelUi` | false | In a pixel project, a Stage pixelates its HUD too; off keeps it sharp |
| `font` | empty | The font Painter draws with when given none, from `Game/Assets` ([Text](Graphics/Text.md)) |
| `loadingScreen` | Default | The engine's loading screen: Default, Horror, Rpg, SciFi, Arcade or Cozy ([Runtime](Runtime.md#startup-loading-and-error-boundary)) |

The schema is [ProjectSettings.schema.json](../Core/Runtime/ProjectSettings.schema.json): bounds, notes and groups
come from it, and the committed [ProjectSettings.h](../Core/Runtime/ProjectSettings.h) is generated from it.

The application id names the folder that keeps the players' options and saves. After a release, a new id starts every
player with empty options and no saves, so the editor asks before it changes.

## How a game reads them

`GameSettings::project` starts as the project's settings; the rest of `GameSettings` (sounds, theme, looks, audio
catalog, save policy) stays in code. A game never sets `project` fields: the file is their only source. Engine hosts do
(the editor's worker, tools, tests), and a worker may give itself an id of its own:

```cpp
GameSettings settings = MyProjectSettings();
settings.project.applicationId = "MyProjectWorker";
```

| | Development (Debug, editor) | Release configurations |
| --- | --- | --- |
| Value source | The built values, then the file once more when the program starts | Compiled in |
| A change | Applies at the next start; in the editor, Build & Refresh | Needs a build |
| An invalid file | The built values, with the error on stderr | The build stops at `file:line` |

Each game and worker build turns the file into a small `.cpp` that hands its values to the engine before `main`
(`engine_add_project_settings`, which `engine_add_game` and `engine_add_editor_worker` call). The game's build also
writes a `.rc` with the icon and the version details. The font is shipped by itself: it need not be listed in
`GameSettings::assets`.

`Version::Parse(CurrentProjectSettings().version)` gives the [Version module](../Core/Modules/Version/README.md) the
same version.

## Format

A flat JSON object of `name: value`, in schema order, one per line; values are written as in a [tuning
file](Tuning.md#file). Unknown names and out-of-bounds values are errors, nothing is clamped. `python Engine check`
reports an invalid or non-canonical file, and `check --fix` rewrites a valid one in schema order.

The schema is written like a tuning file whose values are the defaults. It also allows `text` (one line;
`"format": "version"` checks a version) and `path` (relative, `/` separators, no `.` or `..`), and has neither lists
nor tables. Only a settings schema has these types: tuning values are compiled `constexpr` and patched in place while
the game runs, which a string cannot be.

## Engine side

- Schema: `Core/Runtime/ProjectSettings.schema.json`; `python . check --fix` regenerates `ProjectSettings.h`.
- Values, runtime: `Core/Runtime/ProjectSettingsStore.h` (`CurrentProjectSettings`, `ReadProjectSettings`); parsing
  and patching: `ParseSettingsSchema`, `ParseSettingsValues`, `PatchSettingsValue` in `Core/Tuning/TuningFile.h`.
- Generator and `check`: `Tools/tuning/settings.py`. Rules are implemented twice, in Python and C++; the shared cases in
  `Tools/Tests/SettingsCases/` run on both sides.
