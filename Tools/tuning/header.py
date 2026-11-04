"""The C++ header generated from a tuning file (Docs/Tuning.md → Generated header).

The struct's member defaults are the file's values: Release returns them as `constexpr` from `Tuning<T>()`, and
development starts from them. `VisitTuning` introduces the fields to the live side (Core/Tuning/TuningLive.h). A choice
becomes an `enum class` named after its field, with its names beside it, so the file stays the only list of them.
"""

import re
from pathlib import Path, PurePosixPath

from .document import Field, Spec, TuningError, read_fields, type_name

SUFFIX = ".tuning.json"
STRUCT_STEM = re.compile(r"[A-Z][A-Za-z0-9]*\Z")
CPP_TYPES = {"float": "float", "int": "int", "bool": "bool", "color": "Color4", "vec2": "Float2",
             "range": "FloatRange"}
TYPE_HEADERS = {"color": "Utilities/Color4.h", "vec2": "Utilities/Float2.h", "range": "Utilities/FloatRange.h"}


def struct_name(source: Path) -> str:
    """`WorldConfig.tuning.json` → `WorldConfigTuning`."""
    if not source.name.endswith(SUFFIX):
        raise TuningError(1, f"a tuning file is named <Name>{SUFFIX}")
    stem = source.name[:-len(SUFFIX)]
    if not STRUCT_STEM.match(stem):
        raise TuningError(1, f'file name "{stem}" must be PascalCase: it names the struct {stem}Tuning')
    return stem + "Tuning"


def project_path(source: Path, root: Path) -> str:
    """The file's path from the project root, as the development executable finds it (TuningSource)."""
    try:
        return PurePosixPath(source.resolve().relative_to(root.resolve()).as_posix()).as_posix()
    except ValueError as error:
        raise TuningError(1, f"the tuning file is outside the project folder {root.as_posix()}") from error


def _float(value: float) -> str:
    text = repr(float(value))
    return text + ("f" if "." in text or "e" in text else ".0f")


def _literal(spec: Spec, enum: str, value) -> str:
    if spec.type == "float":
        return _float(value)
    if spec.type == "int":
        return str(value)
    if spec.type == "bool":
        return "true" if value else "false"
    if spec.type == "choice":
        return f"{enum}::{value}"
    if spec.type == "color":
        return "Color4{" + ", ".join(str(channel) for channel in value) + "}"
    return CPP_TYPES[spec.type] + "{" + ", ".join(_float(number) for number in value) + "}"


def _element_type(spec: Spec, enum: str) -> str:
    return enum if spec.type == "choice" else CPP_TYPES[spec.type]


def _declared_type(spec: Spec, enum: str, length: int) -> str:
    element = _element_type(spec, enum)
    return f"std::array<{element}, {length}>" if spec.is_list else element


def _initializer(spec: Spec, enum: str, value) -> str:
    if spec.is_list:
        return "{" + ", ".join(_literal(spec, enum, item) for item in value) + "}"
    return _literal(spec, enum, value)


def _comment(note: str | None, unit: str | None, indent: str) -> list[str]:
    if not note and not unit:
        return []
    text = (note or "") + (f" ({unit})" if unit else "")
    return [f"{indent}// {text.strip()}"]


def _enum(name: str, spec: Spec, indent: str) -> list[str]:
    """A choice's enum and its names in file order; the live side matches values by name."""
    enum = type_name(name)
    names = ", ".join(f'"{choice}"' for choice in spec.choices)
    return [f"{indent}enum class {enum} : int {{ {', '.join(spec.choices)} }};",
            f"{indent}static constexpr std::array<std::string_view, {len(spec.choices)}> {enum}Names{{{names}}};"]


def _visit(entries: list[tuple[str, str]], indent: str) -> list[str]:
    """`entries`: (name, kind) with kind "field", "table" or "choice"."""
    lines = [f"{indent}template <class Visitor>", f"{indent}void VisitTuning(Visitor& visitor) {{"]
    for name, kind in entries:
        if kind == "choice":
            lines.append(f'{indent}    visitor.Choice("{name}", {name}, {type_name(name)}Names);')
        else:
            lines.append(f'{indent}    visitor.{"Table" if kind == "table" else "Field"}("{name}", {name});')
    return lines + [f"{indent}}}"]


def _kind(spec: Spec) -> str:
    return "choice" if spec.type == "choice" else "field"


def _row_struct(field: Field) -> list[str]:
    row = type_name(field.name) + "Row"
    lines = [f"    struct {row} {{"]
    for column, spec in field.columns:
        if spec.type == "choice":
            lines += _enum(column, spec, "        ")
    for column, spec in field.columns:
        length = len(field.rows[0][column]) if spec.is_list else 0
        lines += _comment(spec.note, spec.unit, "        ")
        lines.append(f"        {_declared_type(spec, type_name(column), length)} {column}{{}};")
    lines.append("")
    lines += _visit([(column, _kind(spec)) for column, spec in field.columns], "        ")
    return lines + ["    };"]


def _member(field: Field) -> list[str]:
    if field.is_table:
        row = type_name(field.name) + "Row"
        lines = _comment(field.note, None, "    ") + [f"    std::array<{row}, {len(field.rows)}> {field.name}{{"]
        for row_values in field.rows:
            cells = ", ".join(_initializer(spec, f"{row}::{type_name(column)}", row_values[column])
                              for column, spec in field.columns)
            lines.append(f"        {row}{{{cells}}},")
        return lines + ["    };"]
    enum = type_name(field.name)
    length = len(field.value) if field.spec.is_list else 0
    declared = _declared_type(field.spec, enum, length)
    lines = _comment(field.note, field.spec.unit, "    ")
    if field.spec.is_list:
        return lines + [f"    {declared} {field.name}{_initializer(field.spec, enum, field.value)};"]
    return lines + [f"    {declared} {field.name} = {_initializer(field.spec, enum, field.value)};"]


def _escaped(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def render(fields: list[Field], struct: str, source: str) -> str:
    """`source` is the file's path from the project root."""
    specs = [spec for field in fields for spec in ([field.spec] if field.spec else [s for _, s in field.columns])]
    needs_array = any(spec.is_list or spec.type == "choice" for spec in specs) or any(f.is_table for f in fields)
    includes = (["<array>"] if needs_array else []) + ["<string_view>"]
    project = sorted({TYPE_HEADERS[spec.type] for spec in specs if spec.type in TYPE_HEADERS})
    lines = [f"// Generated from {PurePosixPath(source).name} by the engine's tuning generator (Tools/tuning). Do not "
             "edit: change", "// the JSON; values apply live in development builds and are compiled in for Release.",
             "#pragma once"]
    lines += [f"#include {header}" for header in includes]
    lines += [f'#include "{header}"' for header in ["Tuning/Tuning.h"] + project]
    lines += ["", f"struct {struct} {{"]
    enums = [line for field in fields if field.spec is not None and field.spec.type == "choice"
             for line in _enum(field.name, field.spec, "    ")]
    lines += enums + ([""] if enums else [])
    for field in fields:
        if field.is_table:
            lines += _row_struct(field) + [""]
    for field in fields:
        lines += _member(field)
    lines += ["#if ENGINE_TUNING_LIVE",
              f'    static constexpr std::u8string_view TuningSource = u8"{_escaped(source)}";',
              "#endif", ""]
    lines += _visit([(field.name, "table" if field.is_table else _kind(field.spec)) for field in fields], "    ")
    return "\n".join(lines + ["};", ""])


def generate(source: Path, header: Path, root: Path) -> bool:
    """Writes the header; leaves it untouched when the content is the same (no needless rebuild). True if written."""
    struct = struct_name(source)
    relative = project_path(source, root)
    try:
        text = source.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        raise TuningError(1, f"the file is not UTF-8: {error}") from error
    content = render(read_fields(text), struct, relative)
    if header.is_file() and header.read_text(encoding="utf-8") == content:
        return False
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(content, encoding="utf-8", newline="\n")
    return True
