"""Settings files (Docs/ProjectSettings.md): an engine schema and a project's values file.

The schema (`Core/Runtime/ProjectSettings.schema.json`) is written like a tuning file whose values are the defaults; it
also allows `text` and `path`, and has neither lists nor tables. `python . check --fix` turns it into the committed
`Core/Runtime/ProjectSettings.h`. A project's `Game/Game.settings.json` is a flat object holding only the values that
differ from the defaults, in schema order. Each game and worker build turns it into a small `.cpp` that hands the values
to the engine, and the game's build also into a `.rc` with the icon and version details.

The same rules run in C++ (Core/Tuning/TuningFile.cpp: ParseSettingsSchema, ParseSettingsValues); the shared cases in
Tools/Tests/SettingsCases check both sides.
"""

from pathlib import Path, PurePosixPath

from .document import Field, Node, TuningError, _checked, parse_json, read_fields, type_name
from .header import _float
from .layout import _inline, _key

ENGINE_ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ENGINE_ROOT / "Core" / "Runtime" / "ProjectSettings.schema.json"
HEADER = ENGINE_ROOT / "Core" / "Runtime" / "ProjectSettings.h"
VALUES = PurePosixPath("Game/Game.settings.json")  # From the project folder
STRUCT = "ProjectSettings"
CPP_TYPES = {"float": "float", "int": "int", "bool": "bool", "color": "Color4", "vec2": "Float2",
             "range": "FloatRange", "text": "std::string", "path": "std::string"}
TYPE_HEADERS = {"color": "Utilities/Color4.h", "vec2": "Utilities/Geometry/Float2.h",
                "range": "Utilities/Math/FloatRange.h"}


def read_schema(text: str | None = None) -> list[Field]:
    return read_fields(SCHEMA.read_text(encoding="utf-8") if text is None else text, schema=True)


def _entries(text: str, schema: list[Field]) -> list[tuple[Field, Node, int]]:
    """The file's (field, value node, key line) in schema order, after validating every value."""
    root = parse_json(text)
    if root.kind != "object":
        raise TuningError(root.line, "a settings file is a JSON object of values")
    known = {field.name: field for field in schema}
    entries = []
    for name, line, node in root.value:
        if name not in known:
            raise TuningError(line, f'unknown setting "{name}"')
        _checked(node, known[name].spec, f'"{name}"')
        entries.append((known[name], node, line))
    order = [field.name for field in schema]
    return sorted(entries, key=lambda entry: order.index(entry[0].name))


def read_values(text: str, schema: list[Field]) -> dict[str, object]:
    """Every setting's value: the file's, else the schema's default."""
    values = {field.name: field.value for field in schema}
    for field, node, _ in _entries(text, schema):
        values[field.name] = _checked(node, field.spec, f'"{field.name}"')
    return values


def value_lines(text: str, schema: list[Field]) -> dict[str, int]:
    return {field.name: line for field, _, line in _entries(text, schema)}


def canonical_values(text: str, schema: list[Field]) -> str:
    entries = _entries(text, schema)
    if not entries:
        return "{}\n"
    lines = [f"  {_key(field.name)}: {_inline(node)}" for field, node, _ in entries]
    return "{\n" + ",\n".join(lines) + "\n}\n"


# --- C++ ---------------------------------------------------------------------------------------------------------

def _string(text: str) -> str:
    """A C++ narrow string literal holding the UTF-8 bytes, whatever the compiler's source charset."""
    out = []
    for byte in text.encode("utf-8"):
        char = chr(byte)
        if char in '"\\':
            out.append("\\" + char)
        elif 0x20 <= byte < 0x7F:
            out.append(char)
        else:
            out.append(f"\\{byte:03o}")  # Octal: at most three digits, so the next character cannot extend it
    return '"' + "".join(out) + '"'


def _literal(field: Field, value) -> str:
    kind = field.spec.type
    if kind in ("text", "path"):
        return _string(value)
    if kind == "float":
        return _float(value)
    if kind == "int":
        return str(value)
    if kind == "bool":
        return "true" if value else "false"
    if kind == "choice":
        return f"{STRUCT}::{type_name(field.name)}::{value}"
    if kind == "color":
        return "Color4{" + ", ".join(str(channel) for channel in value) + "}"
    return CPP_TYPES[kind] + "{" + ", ".join(_float(number) for number in value) + "}"


def _declared(field: Field) -> str:
    return type_name(field.name) if field.spec.type == "choice" else CPP_TYPES[field.spec.type]


def _comment(field: Field) -> list[str]:
    if not field.note:
        return []
    return [f"    // {field.note}" + (f" ({field.spec.unit})" if field.spec.unit else "")]


def render_header(schema: list[Field], schema_text: str) -> str:
    """The committed `ProjectSettings.h`: members with the schema's defaults, the schema text for the runtime and the
    editor, and `VisitSettings`."""
    includes = sorted({TYPE_HEADERS[f.spec.type] for f in schema if f.spec.type in TYPE_HEADERS})
    needs_array = any(f.spec.type == "choice" for f in schema)
    lines = ["// Generated from ProjectSettings.schema.json by the engine's settings tools (Tools/tuning/settings.py).",
             "// Do not edit: change the schema and run `python . check --fix`.",
             "#pragma once"]
    lines += (["#include <array>"] if needs_array else []) + ["#include <string>", "#include <string_view>"]
    lines += [f'#include "{header}"' for header in includes]
    lines += ["",
              "// The project's settings (Docs/ProjectSettings.md): `Game/Game.settings.json` holds the values that differ",
              "// from these defaults, and the editor edits them under Project > Project settings. A game reads them as",
              "// GameSettings::project; engine hosts (the editor's worker, tools, tests) may change them in code.",
              f"struct {STRUCT} {{"]
    for field in schema:
        if field.spec.type == "choice":
            names = ", ".join(f'"{choice}"' for choice in field.spec.choices)
            enum = type_name(field.name)
            lines += [f"    enum class {enum} : int {{ {', '.join(field.spec.choices)} }};",
                      f"    static constexpr std::array<std::string_view, {len(field.spec.choices)}> {enum}Names{{{names}}};"]
    group = None
    for index, field in enumerate(schema):
        if field.group != group or index == 0:
            group = field.group
            lines += ([""] if index or needs_array else []) + ([f"    // {group}"] if group else [])
        lines += _comment(field)
        lines.append(f"    {_declared(field)} {field.name} = {_literal(field, field.value)};")
    delimiter = "schema"
    assert f"){delimiter}\"" not in schema_text
    lines += ["", f"    friend bool operator==(const {STRUCT}&, const {STRUCT}&) = default;"]
    lines += ["", f"    static constexpr std::string_view Schema = R\"{delimiter}(" + schema_text + f"){delimiter}\";", "",
              "    template <class Visitor>", "    void VisitSettings(Visitor& visitor) {"]
    for field in schema:
        if field.spec.type == "choice":
            lines.append(f'        visitor.Choice("{field.name}", {field.name}, {type_name(field.name)}Names);')
        else:
            lines.append(f'        visitor.Field("{field.name}", {field.name});')
    lines += ["    }", "};", ""]
    return "\n".join(lines)


def render_values(schema: list[Field], values: dict[str, object], source: str) -> str:
    """The `.cpp` a game or worker target compiles: it hands the file's values to the engine before `main`."""
    lines = [f"// Generated from {source} by the engine's settings tools (Tools/tuning/settings.py). Do not edit.",
             '#include "Runtime/ProjectSettingsStore.h"', "", "namespace {", f"{STRUCT} Compiled() {{",
             f"    {STRUCT} settings;"]
    for field in schema:
        if values[field.name] != field.value:
            lines.append(f"    settings.{field.name} = {_literal(field, values[field.name])};")
    lines += ["    return settings;", "}", "",
              "// Development builds read the file again at startup, so an edit needs no build (ENGINE_TUNING_LIVE).",
              "#if ENGINE_TUNING_LIVE",
              "const bool Registered = (RegisterProjectSettings(Compiled(), true), true);",
              "#else",
              "const bool Registered = (RegisterProjectSettings(Compiled(), false), true);",
              "#endif",
              "} // namespace", ""]
    return "\n".join(lines)


def _rc_string(text: str) -> str:
    return '"' + text.replace('"', '""') + '"'


def render_rc(values: dict[str, object], project: Path, line: dict[str, int], source: str) -> str:
    """The game's resources: `GLFW_ICON` (window, taskbar and the exe in Explorer) and the version details."""
    lines = ["// Generated from " + source + " by the engine's settings tools (Tools/tuning/settings.py). Do not edit.",
             "#pragma code_page(65001)"]
    if values["icon"]:
        icon = (project / values["icon"]).resolve()
        if not icon.is_file():
            raise TuningError(line.get("icon", 1), f'icon "{values["icon"]}" is not a file in the project folder')
        if icon.suffix.lower() != ".ico":
            raise TuningError(line.get("icon", 1), f'icon "{values["icon"]}" must be a Windows .ico file')
        lines.append(f"GLFW_ICON ICON {_rc_string(icon.as_posix())}")
    numbers = [int(part) for part in str(values["version"]).split(".")]
    numbers += [0] * (4 - len(numbers))
    version = ",".join(str(number) for number in numbers)
    strings = [("CompanyName", values["company"]), ("FileDescription", values["title"]),
               ("FileVersion", values["version"]), ("ProductName", values["title"]),
               ("ProductVersion", values["version"])]
    lines += ["1 VERSIONINFO", f"FILEVERSION {version}", f"PRODUCTVERSION {version}", "FILEFLAGSMASK 0x3fL",
              "FILEFLAGS 0x0L", "FILEOS 0x40004L", "FILETYPE 0x1L", "FILESUBTYPE 0x0L", "BEGIN",
              '  BLOCK "StringFileInfo"', "  BEGIN", '    BLOCK "040904b0"', "    BEGIN"]
    lines += [f'      VALUE "{key}", {_rc_string(str(text))}' for key, text in strings if text]
    lines += ["    END", "  END", '  BLOCK "VarFileInfo"', "  BEGIN", '    VALUE "Translation", 0x409, 1200', "  END",
              "END", ""]
    return "\n".join(lines)


def _write(path: Path, content: str) -> bool:
    if path.is_file() and path.read_text(encoding="utf-8") == content:
        return False
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8", newline="\n")
    return True


def generate(values_file: Path, cpp: Path, rc: Path | None, project: Path) -> None:
    """Build step. An absent values file means every default."""
    schema = read_schema()
    text = "{}"
    if values_file.is_file():
        try:
            text = values_file.read_text(encoding="utf-8")
        except UnicodeDecodeError as error:
            raise TuningError(1, f"the file is not UTF-8: {error}") from error
    values = read_values(text, schema)
    source = VALUES.as_posix()
    _write(cpp, render_values(schema, values, source))
    if rc is not None:
        _write(rc, render_rc(values, project, value_lines(text, schema), source))


def header_problem(fix: bool) -> str | None:
    """Whether the committed header matches the schema; `fix` rewrites it."""
    schema_text = SCHEMA.read_text(encoding="utf-8")
    expected = render_header(read_schema(schema_text), schema_text.rstrip("\n"))
    if HEADER.is_file() and HEADER.read_text(encoding="utf-8").replace("\r\n", "\n") == expected:
        return None
    if fix:
        HEADER.write_text(expected, encoding="utf-8", newline="\n")
        return None
    return f"{HEADER.relative_to(ENGINE_ROOT).as_posix()}: out of date with the schema (fix: check --fix)"


def check_settings(root: Path, fix: bool) -> tuple[list[str], list[str]]:
    """`python Engine check`: the engine's committed header is current, and the project's values file is valid and
    canonical. (problems, rewritten files); `fix` rewrites what it can."""
    problems, fixed = [], []
    root = root.resolve()
    if root == ENGINE_ROOT:
        try:
            before = HEADER.read_text(encoding="utf-8") if HEADER.is_file() else None
            problem = header_problem(fix)
            if problem:
                problems.append(problem)
            elif fix and HEADER.read_text(encoding="utf-8") != before:
                fixed.append(HEADER.relative_to(ENGINE_ROOT).as_posix())
        except TuningError as error:
            problems.append(f"{SCHEMA.relative_to(ENGINE_ROOT).as_posix()}:{error.line}: {error.text}")
            return problems, fixed
    values = root / VALUES
    if not values.is_file():
        return problems, fixed
    where = VALUES.as_posix()
    try:
        text = values.read_bytes().decode("utf-8")
        expected = canonical_values(text, read_schema())
    except UnicodeDecodeError:
        return problems + [f"{where}:1: the file is not UTF-8"], fixed
    except TuningError as error:
        return problems + [f"{where}:{error.line}: {error.text}"], fixed
    if text.replace("\r\n", "\n") != expected:
        if fix:
            values.write_text(expected, encoding="utf-8", newline="\n")
            fixed.append(where)
        else:
            problems.append(f"{where}: not in the canonical layout (fix: check --fix)")
    return problems, fixed
