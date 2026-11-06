"""Input map files (Docs/InputMap.md): `X.input.json` names a game's buttons and axes and the default keys per player.

```json
{
  "buttons": ["Left", "Right", "Jump"],
  "axes": ["MoveX"],
  "players": [
    { "Left": ["A", "Left"], "Right": ["D", "Right"], "Jump": ["Space"] }
  ]
}
```

The build turns it into `X.input.h`: `struct XInput` with `enum class Button` / `Axis`, their names and the default
keys. Buttons and axes are structural (the enums; a new list needs a build), keys are values (development builds read
them from the file when a scene binds them). Key names are those of `enum class InputKey` in Core/Input/Input.h.

The same rules run in C++ (Core/Input/Internals/InputMap.cpp); the shared cases in Tools/Tests/InputMapCases check both.
"""

import re
from dataclasses import dataclass, field
from pathlib import Path, PurePosixPath

from .document import CHOICE_NAME, Node, TuningError, parse_json
from .header import _escaped, project_path
from .layout import _key

SUFFIX = ".input.json"
STRUCT_STEM = re.compile(r"[A-Z][A-Za-z0-9]*\Z")
TOP_KEYS = ("buttons", "axes", "players")
MAX_BUTTONS = 32  # Simulation/PlayerInput.h MaxPlayerButtons
MAX_AXES = 8  # MaxPlayerAxes
MAX_PLAYERS = 16
INPUT_HEADER = Path(__file__).resolve().parents[2] / "Core" / "Input" / "Input.h"


def key_names() -> list[str]:
    """`enum class InputKey` in Core/Input/Input.h, in order, without Count."""
    text = INPUT_HEADER.read_text(encoding="utf-8")
    body = re.search(r"enum class InputKey\s*\{(.*?)\}", text, re.S).group(1)
    names = [name.strip() for name in body.split(",") if name.strip()]
    return [name for name in names if name != "Count"]


@dataclass
class InputMap:
    buttons: list[str]
    axes: list[str] = field(default_factory=list)
    players: list[dict[str, list[str]]] = field(default_factory=list)  # button → key names, in button order
    lines: dict[str, int] = field(default_factory=dict)


def _names(node: Node, what: str, limit: int, at_least: int) -> list[str]:
    if node.kind != "array":
        raise TuningError(node.line, f'"{what}" must be a list of names')
    if len(node.value) < at_least:
        raise TuningError(node.line, f'"{what}" needs at least {at_least} name')
    if len(node.value) > limit:
        raise TuningError(node.line, f'"{what}" has more than {limit} names')
    names = []
    for item in node.value:
        if item.kind != "string" or not CHOICE_NAME.match(item.value):
            shown = item.value if item.kind == "string" else "a non-text value"
            raise TuningError(item.line, f'"{what}" name "{shown}" must be PascalCase: an uppercase letter, then '
                                         "letters and digits")
        if item.value in names:
            raise TuningError(item.line, f'"{what}" lists "{item.value}" twice')
        names.append(item.value)
    return names


def _player(node: Node, index: int, buttons: list[str], keys: set[str]) -> dict[str, list[str]]:
    where = f"player {index}"
    if node.kind != "object":
        raise TuningError(node.line, f"{where} must be an object of button keys")
    bound = {}
    for button, line, value in node.value:
        if button not in buttons:
            raise TuningError(line, f'{where} binds unknown button "{button}"')
        if value.kind != "array":
            raise TuningError(value.line, f'{where} "{button}" must be a list of key names')
        names = []
        for item in value.value:
            if item.kind != "string" or item.value not in keys:
                shown = item.value if item.kind == "string" else "a non-text value"
                raise TuningError(item.line, f'{where} "{button}": "{shown}" is not a key name (Input/Input.h InputKey)')
            if item.value in names:
                raise TuningError(item.line, f'{where} "{button}" lists "{item.value}" twice')
            names.append(item.value)
        bound[button] = names
    return {button: bound[button] for button in buttons if button in bound}


def read_input_map(text: str) -> InputMap:
    root = parse_json(text)
    if root.kind != "object":
        raise TuningError(root.line, "an input map is a JSON object with buttons, axes and players")
    parts = {}
    for key, line, node in root.value:
        if key not in TOP_KEYS:
            raise TuningError(line, f'unknown key "{key}" (allowed: {", ".join(TOP_KEYS)})')
        parts[key] = (line, node)
    for required in ("buttons", "players"):
        if required not in parts:
            raise TuningError(root.line, f'an input map needs "{required}"')
    result = InputMap(_names(parts["buttons"][1], "buttons", MAX_BUTTONS, 1))
    if "axes" in parts:
        result.axes = _names(parts["axes"][1], "axes", MAX_AXES, 0)
    players = parts["players"][1]
    if players.kind != "array" or not players.value:
        raise TuningError(players.line, '"players" must be a non-empty list')
    if len(players.value) > MAX_PLAYERS:
        raise TuningError(players.line, f'"players" has more than {MAX_PLAYERS} players')
    keys = set(key_names())
    result.players = [_player(node, index, result.buttons, keys) for index, node in enumerate(players.value)]
    return result


def text_of(input_map: InputMap) -> str:
    """The canonical layout."""
    def names(items: list[str]) -> str:
        return "[" + ", ".join(_key(item) for item in items) + "]"

    lines = ["{", f'  "buttons": {names(input_map.buttons)},', f'  "axes": {names(input_map.axes)},', '  "players": [']
    for index, player in enumerate(input_map.players):
        cells = ", ".join(f"{_key(button)}: {names(keys)}" for button, keys in player.items())
        lines.append("    {" + (f" {cells} " if cells else "") + "}" + ("," if index < len(input_map.players) - 1 else ""))
    return "\n".join(lines + ["  ]", "}", ""])


def canonical(text: str) -> str:
    return text_of(read_input_map(text))


def struct_name(source: Path) -> str:
    """`Arena.input.json` → `ArenaInput`."""
    if not source.name.endswith(SUFFIX):
        raise TuningError(1, f"an input map is named <Name>{SUFFIX}")
    stem = source.name[:-len(SUFFIX)]
    if not STRUCT_STEM.match(stem):
        raise TuningError(1, f'file name "{stem}" must be PascalCase: it names the struct {stem}Input')
    return stem + "Input"


def render(input_map: InputMap, struct: str, source: str) -> str:
    def names(items: list[str]) -> str:
        return ", ".join(f'"{item}"' for item in items)

    keys = [(index, button, key) for index, player in enumerate(input_map.players)
            for button, names_ in player.items() for key in names_]
    lines = [f"// Generated from {PurePosixPath(source).name} by the engine's input map tools (Tools/tuning/inputmap.py). "
             "Do not edit:", "// change the JSON; keys apply live in development builds (Docs/InputMap.md).",
             "#pragma once", "#include <array>", "#include <string_view>", "", f"struct {struct} {{",
             f"    enum class Button : int {{ {', '.join(input_map.buttons)} }};",
             f"    enum class Axis : int {{ {', '.join(input_map.axes)} }};",
             f"    static constexpr std::array<std::string_view, {len(input_map.buttons)}> ButtonNames{{{names(input_map.buttons)}}};",
             f"    static constexpr std::array<std::string_view, {len(input_map.axes)}> AxisNames{{{names(input_map.axes)}}};",
             f"    static constexpr int Players = {len(input_map.players)};",
             "    // A default key: player, button and key name (Input/Input.h InputKey).",
             "    struct Key {", "        int player = 0;", "        Button button{};", "        std::string_view key;", "    };"]
    if keys:
        lines.append(f"    static constexpr std::array<Key, {len(keys)}> Keys{{{{")
        lines += [f'        Key{{{player}, Button::{button}, "{key}"}},' for player, button, key in keys]
        lines.append("    }};")
    else:
        lines.append("    static constexpr std::array<Key, 0> Keys{};")
    lines += ["#if ENGINE_TUNING_LIVE", f'    static constexpr std::u8string_view InputSource = u8"{_escaped(source)}";',
              "#endif", "};", ""]
    return "\n".join(lines)


def generate(source: Path, header: Path, root: Path) -> bool:
    struct = struct_name(source)
    relative = project_path(source, root)
    try:
        text = source.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        raise TuningError(1, f"the file is not UTF-8: {error}") from error
    content = render(read_input_map(text), struct, relative)
    if header.is_file() and header.read_text(encoding="utf-8") == content:
        return False
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(content, encoding="utf-8", newline="\n")
    return True
