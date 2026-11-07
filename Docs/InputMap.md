# Input map

An input map names a game's buttons and axes once and gives each player's default keys: `X.input.json` beside the
simulation code. The build turns it into `X.input.h`, so the enum the simulation reads, the names its tapes record and
the keys a scene binds cannot drift apart ([D52](Design/README.md#d52-input-map)). Edit the keys in the editor under
**Project > Input map**, by hand or with an agent.

`Game/Simulation/Arena.input.json`:

```json
{
  "buttons": ["Left", "Right", "Jump"],
  "axes": ["MoveX"],
  "players": [
    { "Left": ["A", "Left"], "Right": ["D", "Right"], "Jump": ["Space"] },
    { "Left": ["J"], "Right": ["L"], "Jump": ["RightControl"] }
  ]
}
```

- `buttons`: 1 to 32 PascalCase names; `axes`: up to 8, and the game sets their values (`PlayerInput::SetAxis`).
- `players`: one object per player; a button may have any number of keys, or none. Key names are those of
  `enum class InputKey` in [Input.h](../Core/Input/Input.h): `A`, `Zero`, `Up`, `Space`, `LeftShift`, `F1`... and the
  mouse buttons `MouseLeft`, `MouseRight`, `MouseMiddle` (`"Aim": ["MouseRight"]`). The Inspector binds a mouse
  button when you click its "Press a key" prompt with that button.
- One canonical layout; `python Engine check` reports others and `check --fix` rewrites them.

## Code

`engine_add_tuning(<target> ROOT <folder>)` generates the header for every `*.input.json` under the root, as it does
for tuning files; the project template already calls it for `Game/`. The file name names the struct:
`Arena.input.json` → `ArenaInput`.

```cpp
#include "Simulation/Arena.input.h"

// The simulation's declaration takes the names from the map:
SimulationSettings settings;
DeclareInput<ArenaInput>(settings);

// The simulation reads the generated enum:
if (input.Held(ArenaInput::Button::Jump)) Jump();

// A scene binds the default keys for the players it reads:
InputBindings bindings(2);
bindings.BindMap<ArenaInput>();
```

| | Development (Debug, editor) | Release configurations |
| --- | --- | --- |
| Keys | Read from the file each time a scene binds the map: a change applies at the next scene start (Reset) | Compiled in |
| Buttons and axes | The generated enums; a new list needs a build, and until then the built keys stay, with a warning | Compiled in |

Tapes hold actions, not keys, so a key change never touches a replay. Reordering or renaming buttons changes what old
tapes mean: raise `SimulationSettings::version` ([Simulation](Simulation.md)).

Not yet: gamepads, an axis from a pair of keys, and players changing their keys in the game (keys saved in the
options file, a rebinding widget).

## Engine side

- Parsing, patching and key names: `Core/Input/InputMap.h`; binding: `InputBindings::BindMap`; declaring:
  `DeclareInput` in `Core/Simulation/SimulationSettings.h`.
- Generator and `check`: `Tools/tuning/inputmap.py`. Rules are implemented twice, in Python and C++; the shared cases in
  `Tools/Tests/InputMapCases/` run on both sides.
