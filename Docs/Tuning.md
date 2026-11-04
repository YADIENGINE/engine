# Tuning files

Manually tuned numbers (radius, speed, color, duration, probability, price) can live in an `X.tuning.json` file beside
the class instead of C++ constants. When a value changes in the editor or file, the running game picks it up without
building; Release compiles the same values as `constexpr`. JSON is the single source: visible in git, editable by hand,
editor, or AI. The system is opt-in; design decisions are in `Docs/Design/Tuning.md` in the engine repo.

Structural constants (array sizes, slot counts, enum-related numbers) stay in code, outside tuning. Existing `constexpr`
constants do not need to move; both methods coexist.

## Setup

The target declares once which folder to search for tuning files:

```cmake
engine_add_tuning(MyGame ROOT Game)
```

Each `*.tuning.json` under `ROOT` generates `<build>/Tuning/MyGame/<path relative to ROOT>.tuning.h` during the build; that
folder is the target's include directory. New files are not listed in CMake; the next build finds them. Generated
headers are not tracked in the repo. Two tuning files with the same name in one target are an error (structs are
global). The game target in a project created by `python Engine new` already calls this. The generator requires Python
3.10+ (the same as `python Engine`).

## File

`Game/Effects/Glow.tuning.json`:

```json
{
  "radius": { "value": 12.0, "min": 0, "max": 40, "unit": "px", "note": "Glow radius at rest" },
  "pulses": { "type": "int", "value": 3, "min": 1 },
  "mode": { "type": "choice", "choices": ["Steady", "Pulse"], "value": "Pulse", "group": "Motion" },
  "tint": { "type": "color", "value": "#FF4080" },
  "offset": { "type": "vec2", "value": [0, -4] },
  "delay": { "type": "range", "value": { "min": 0.2, "max": 0.6 }, "min": 0 },
  "steps": { "type": "float[]", "value": [0, 0.5, 1] },
  "odds": {
    "type": "table",
    "group": "Balance",
    "columns": {
      "fromLevel": { "type": "int", "min": 1 },
      "chances": { "type": "float[]", "min": 0, "max": 100 }
    },
    "rows": [
      { "fromLevel": 1, "chances": [70, 30] },
      { "fromLevel": 5, "chances": [40, 60] }
    ]
  }
}
```

| `type` | Value | C++ |
| --- | --- | --- |
| (absent, number) / `float` | `12.0` | `float` |
| `int` | `3` | `int` |
| (absent, true/false) / `bool` | `true` | `bool` |
| `color` | `"#RRGGBB"` / `"#RRGGBBAA"` | `Color4` (`Utilities/Color4.h`; `ToColor` when drawing) |
| `vec2` | `[x, y]` | `Float2` |
| `range` | `{ "min": a, "max": b }` | `FloatRange` |
| `choice` | one of `choices` | `enum class <Field> : int`, generated |
| `T[]` | nonempty list | `std::array<T, N>` |
| `table` | `columns` + `rows` | `std::array<<Field>Row, N>` |

- Field and column names are camelCase and become struct members. The filename is PascalCase and names the struct:
  `Glow.tuning.json` → `GlowTuning`.
- If `type` is omitted, a number becomes `float`, `true`/`false` becomes `bool` (`3` does not automatically become
  `int`); it is required for other types.
- `min` / `max` are validity bounds (not slider ranges): applied to every list element and every number in `vec2` /
  `range`; `bool` and `color` do not accept bounds.
- `choice` lists its names in `choices`: PascalCase, unique, in the order of the generated enum
  (`"mode"` → `GlowTuning::Mode::Pulse`, with `GlowTuning::ModeNames`). The file is the only list of them; a new or
  renamed choice needs a build. A choice's enum and a table's row struct share the struct, so a choice may not be named
  like a table plus `Row`.
- Editor text, all optional and one line each: `note` (what the value does; the Inspector's hover text and a comment in
  the generated header), `unit` (shown after numbers; `float`, `int`, `vec2`, `range` only; at most 16 characters) and
  `group` (the Inspector groups fields under it). A table takes `group` and `note`, a column `note` and `unit`. Labels
  come from the name: `glowRadius` shows as "Glow radius".
- A table column can be a scalar type or list; no nested tables. List column length is the same in every row.
- List length and row count are structural (`std::array` size). Check the length of an enum-related list where used:
  `static_assert(std::tuple_size_v<decltype(GlowTuning::steps)> == StepCount);`
- Cross-field rules such as "chances sum to 100" do not belong in the file; check them in code or tests.

## Code

```cpp
#include "Effects/Glow.tuning.h"
#include "Graphics/Paint/Painter.h"

void GlowView::Draw(Painter& paint) const {
    const auto& tuning = Tuning<GlowTuning>();
    paint.Circle(_center, tuning.radius, Fill(tuning.tint));
}
```

`Tuning<T>()` is identical in both modes and returns `const T&`; use the reference within the frame, do not retain it
(values refresh during development). Tests read values from the struct instead of writing literals.

| | Development (Debug, editor) | Release configurations |
| --- | --- | --- |
| Value source | First `Tuning<T>()` reads the file; new values if the file changed since compilation | Compiled `constexpr` |
| Which file | Its path from the project folder, found from the executable | — |
| When the file changes | Game checks at each frame's start (at most once per 200 ms), applies all or nothing | — |
| Cost | Reading a struct field | Constant |

The mode is the `ENGINE_TUNING_LIVE` definition (0 in Release, MinSizeRel, RelWithDebInfo), read where `Tuning<T>()` is
compiled. The engine libraries always carry the live code, and a Release game simply registers nothing with it.
The generated header records the file's path from the project folder. A development executable finds the project as the
nearest folder holding `Project.json` above the executable, then above the working directory, so moving the project
needs no rebuild; `SetTuningRoot` (`Tuning/TuningLive.h`) overrides it. Live values refresh on the main thread.

## When changes require a build

- Changing an existing value: no build.
- Adding or removing fields / columns, changing types, list lengths, or row counts: the struct changes; the running game
  does not apply the file and reports "build to apply". Building resolves it.

## Errors

No value is silently clamped or partially applied.

- **Build:** invalid JSON, wrong types, out-of-bounds values, or missing columns stop the build with `file:line:
  error: ...`.
- **Runtime:** invalid files are not applied; the last valid values continue. Errors are written to stderr (Output and
  the Inspector in the editor) with `file:line`.
- **Startup:** if the file is invalid, the game starts with compiled values and warns.

## Editor

Tuning values are edited in the Inspector, beside the scene's own controls ([Editor usage](Editor/Usage.md)):

- **Which files:** a scene shows the tuning files it has read since it opened, one section each; `Tuning<T>()` tells
  the editor. Any other file opens from the Inspector's file list or by clicking it in Assets. The list scans the
  `engine_add_tuning` roots, so a new file shows before a build has made its header ("Not built").
- **Editing:** numbers are dragged or typed, within `min` / `max`. While a value is dragged the running scene shows it
  at once without the file changing; when the drag ends the value is patched into the file as a one-line change, and
  the scene rereads it. The file stays the only place a value is kept: hand and agent edits take the same path.
- **Modified** compares with the last commit (git HEAD): a changed field is marked, the reset button and the context menu
  write the committed values back. Without git or a commit nothing is marked.
- **Undo:** Ctrl+Z / Ctrl+Y in the Inspector undo and redo the editor's own writes, as long as the file still holds what
  the edit wrote.
- **Lengths:** `+` and `-` add a copy of the last element or row, or remove one. A new length changes the struct, so the
  running scene reports "Build needed" until Build & Refresh.
- The section shows the running scene's state for the file: Live, Not applied (with line number), Build needed, Unreadable.

## Tapes

A tape records a digest of every tuning file's values after its first step (`tuning` in the tape, only in development
builds). When a replay disagrees with its hashes, `sim verify` lists the files whose values changed since in
`tuningChanged`: tuning belongs to the build, not to the tape ([Simulation](Simulation.md)).

## Format check

Files have one canonical format: 2-space indentation, fields in file order with one per line, keys within a field in
`type, choices, value, min, max, unit, group, note` order (table: `type, group, note, columns, rows`, each on its own
line; column: `type, choices, min, max, unit, note`), one table row per line. The editor always writes this
format. `python Engine check` reports invalid or noncanonical tuning files; `python Engine check --fix` formats valid
files (numbers retain their written form).

## Engine side

- Generation and checking: `Tools/tuning/` (`python -m Tools.tuning generate <json> <header>`, standard library only).
- Format, validation, patching and resizing: `Core/Tuning/TuningFile.h`; live side (registration, use periods, editor
  preview, digests): `Core/Tuning/TuningLive.h`; access: `Core/Tuning/Tuning.h`.
- Rules are implemented twice, in Python and C++; `Tools/Tests/TuningCases/` contains shared cases run by both sides. A
  rule change updates both implementations and the cases together.
