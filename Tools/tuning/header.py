"""Tuning dosyasından üretilen C++ header'ı (Docs/Tuning.md → Üretilen header'ın yeri).

Struct'ın üye varsayılanları dosyadaki değerlerdir: Release'de `Tuning<T>()` bunları `constexpr` verir, geliştirmede
açılış değeridir. `VisitTuning` alanları çalışma zamanına tanıtır (Core/Tuning/TuningLive.h).
"""

import re
from pathlib import Path

from .document import Field, Spec, TuningError, read_fields

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


def _float(value: float) -> str:
    text = repr(float(value))
    return text + ("f" if "." in text or "e" in text else ".0f")


def _literal(type_name: str, value) -> str:
    if type_name == "float":
        return _float(value)
    if type_name == "int":
        return str(value)
    if type_name == "bool":
        return "true" if value else "false"
    if type_name == "color":
        return "Color4{" + ", ".join(str(channel) for channel in value) + "}"
    return CPP_TYPES[type_name] + "{" + ", ".join(_float(number) for number in value) + "}"


def _declared_type(spec: Spec, length: int) -> str:
    element = CPP_TYPES[spec.type]
    return f"std::array<{element}, {length}>" if spec.is_list else element


def _initializer(spec: Spec, value) -> str:
    if spec.is_list:
        return "{" + ", ".join(_literal(spec.type, item) for item in value) + "}"
    return _literal(spec.type, value)


def _row_type(field: Field) -> str:
    return field.name[0].upper() + field.name[1:] + "Row"


def _visit(names: list[tuple[str, bool]], indent: str) -> list[str]:
    lines = [f"{indent}template <class Visitor>", f"{indent}void VisitTuning(Visitor& visitor) {{"]
    for name, is_table in names:
        lines.append(f'{indent}    visitor.{"Table" if is_table else "Field"}("{name}", {name});')
    return lines + [f"{indent}}}"]


def _row_struct(field: Field) -> list[str]:
    row = _row_type(field)
    lines = [f"    struct {row} {{"]
    for column, spec in field.columns:
        length = len(field.rows[0][column]) if spec.is_list else 0
        lines.append(f"        {_declared_type(spec, length)} {column}{{}};")
    lines.append("")
    lines += _visit([(column, False) for column, _ in field.columns], "        ")
    return lines + ["    };"]


def _member(field: Field) -> list[str]:
    if field.is_table:
        row = _row_type(field)
        lines = [f"    std::array<{row}, {len(field.rows)}> {field.name}{{"]
        for row_values in field.rows:
            cells = ", ".join(_initializer(spec, row_values[column]) for column, spec in field.columns)
            lines.append(f"        {row}{{{cells}}},")
        return lines + ["    };"]
    length = len(field.value) if field.spec.is_list else 0
    declared = _declared_type(field.spec, length)
    if field.spec.is_list:
        return [f"    {declared} {field.name}{_initializer(field.spec, field.value)};"]
    return [f"    {declared} {field.name} = {_initializer(field.spec, field.value)};"]


def _escaped(text: str) -> str:
    return text.replace("\\", "\\\\").replace('"', '\\"')


def render(fields: list[Field], struct: str, source: Path) -> str:
    specs = [spec for field in fields for spec in ([field.spec] if field.spec else [s for _, s in field.columns])]
    includes = ["<array>"] if any(spec.is_list for spec in specs) or any(f.is_table for f in fields) else []
    includes.append("<string_view>")
    project = sorted({TYPE_HEADERS[spec.type] for spec in specs if spec.type in TYPE_HEADERS})
    lines = [f"// Generated from {source.name} by the engine's tuning generator (Tools/tuning). Do not edit: change",
             "// the JSON; values apply live in development builds and are compiled in for Release.",
             "#pragma once"]
    lines += [f"#include {header}" for header in includes]
    lines += [f'#include "{header}"' for header in ["Tuning/Tuning.h"] + project]
    lines += ["", f"struct {struct} {{"]
    for field in fields:
        if field.is_table:
            lines += _row_struct(field) + [""]
    for field in fields:
        lines += _member(field)
    lines += ["#if ENGINE_TUNING_LIVE",
              f'    static constexpr std::u8string_view TuningSource = u8"{_escaped(source.resolve().as_posix())}";',
              "#endif", ""]
    lines += _visit([(field.name, field.is_table) for field in fields], "    ")
    return "\n".join(lines + ["};", ""])


def generate(source: Path, header: Path) -> bool:
    """Header'ı üretir; içerik değişmediyse dosyaya dokunmaz (gereksiz derleme olmasın). Yazdıysa True."""
    struct = struct_name(source)
    try:
        text = source.read_text(encoding="utf-8")
    except UnicodeDecodeError as error:
        raise TuningError(1, f"the file is not UTF-8: {error}") from error
    content = render(read_fields(text), struct, source)
    if header.is_file() and header.read_text(encoding="utf-8") == content:
        return False
    header.parent.mkdir(parents=True, exist_ok=True)
    header.write_text(content, encoding="utf-8", newline="\n")
    return True
