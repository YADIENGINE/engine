"""Tuning dosyası biçimi (Docs/Tuning.md): satır bilgili JSON okuma, doğrulama ve alan modeli.

Aynı kuralları çalışma zamanı C++'ta uygular (Core/Tuning/TuningFile.cpp); iki tarafı Tools/Tests/TuningCases'teki
ortak örnekler denetler. Kurallardan birini değiştiren ikisini birlikte değiştirir.
"""

import json
import re
from dataclasses import dataclass, field

SCALAR_TYPES = ("float", "int", "bool", "color", "vec2", "range")
BOUNDED_TYPES = ("float", "int", "vec2", "range")  # min / max alabilen tipler
FIELD_KEYS = ("type", "value", "min", "max")  # Kanonik sıra da budur
TABLE_KEYS = ("type", "columns", "rows")
COLUMN_KEYS = ("type", "min", "max")
RANGE_KEYS = ("min", "max")
MAX_DEPTH = 32

FIELD_NAME = re.compile(r"[a-z][A-Za-z0-9]*\Z")
TYPE_NAME = re.compile(r"(float|int|bool|color|vec2|range)(\[\])?\Z")
COLOR = re.compile(r"#[0-9A-Fa-f]{6}([0-9A-Fa-f]{2})?\Z")
INTEGER = re.compile(r"-?(0|[1-9][0-9]*)\Z")
NUMBER = re.compile(r"-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?")
INT_MIN, INT_MAX = -(2**31), 2**31 - 1

# Alan adı üretilen struct'ın üyesi olur; küçük harfle başlayan C++ anahtar sözcükleri ad olamaz.
CPP_KEYWORDS = frozenset("""
    alignas alignof and and_eq asm auto bitand bitor bool break case catch char char8_t char16_t char32_t class compl
    concept const consteval constexpr constinit const_cast continue co_await co_return co_yield decltype default
    delete do double dynamic_cast else enum explicit export extern false float for friend goto if inline int long
    mutable namespace new noexcept not not_eq nullptr operator or or_eq private protected public register
    reinterpret_cast requires return short signed sizeof static static_assert static_cast struct switch template this
    thread_local throw true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor
    xor_eq
""".split())


class TuningError(Exception):
    """Dosyadaki ilk hata; `line` 1'den başlar."""

    def __init__(self, line: int, text: str):
        super().__init__(f"{line}: {text}")
        self.line = line
        self.text = text


@dataclass
class Node:
    """Konumlu JSON değeri. object: [(anahtar, anahtar satırı, Node)], array: [Node], number: yazıldığı metin,
    string: çözülmüş metin, bool: bool, null: None."""

    kind: str
    line: int
    value: object


@dataclass
class Spec:
    type: str
    is_list: bool
    min: float | None = None
    max: float | None = None

    @property
    def text(self) -> str:
        return self.type + ("[]" if self.is_list else "")


@dataclass
class Field:
    """Bir tuning alanı. Değer / liste alanında `spec` + `value` (liste için list); tabloda `columns` ve `rows`
    (satır: sütun adı → değer, sütun sırasıyla)."""

    name: str
    line: int
    spec: Spec | None = None
    value: object = None
    columns: list[tuple[str, Spec]] = field(default_factory=list)
    rows: list[dict[str, object]] = field(default_factory=list)

    @property
    def is_table(self) -> bool:
        return self.spec is None


# --- JSON ---------------------------------------------------------------------------------------------------------

class _Reader:
    def __init__(self, text: str):
        self.text = text
        self.at = 0
        self.line = 1

    def fail(self, text: str):
        raise TuningError(self.line, text)

    def skip_space(self):
        while self.at < len(self.text) and self.text[self.at] in " \t\r\n":
            if self.text[self.at] == "\n":
                self.line += 1
            self.at += 1

    def peek(self) -> str:
        return self.text[self.at] if self.at < len(self.text) else ""

    def expect(self, char: str):
        if self.peek() != char:
            self.fail(f"expected '{char}'" if self.peek() else f"expected '{char}' before the end of the file")
        self.at += 1

    def value(self, depth: int) -> Node:
        if depth > MAX_DEPTH:
            self.fail("nesting is too deep")
        self.skip_space()
        char, line = self.peek(), self.line
        if char == "{":
            return self.object(depth)
        if char == "[":
            return self.array(depth)
        if char == '"':
            return Node("string", line, self.string())
        for word, kind, value in (("true", "bool", True), ("false", "bool", False), ("null", "null", None)):
            if self.text.startswith(word, self.at):
                self.at += len(word)
                return Node(kind, line, value)
        match = NUMBER.match(self.text, self.at)
        if match and match.end() > self.at:
            self.at = match.end()
            return Node("number", line, match.group(0))
        self.fail("expected a JSON value" if char else "unexpected end of the file")

    def object(self, depth: int) -> Node:
        node = Node("object", self.line, [])
        self.at += 1
        self.skip_space()
        if self.peek() == "}":
            self.at += 1
            return node
        seen = set()
        while True:
            self.skip_space()
            if self.peek() != '"':
                self.fail("expected a quoted key")
            key_line = self.line
            key = self.string()
            if key in seen:
                raise TuningError(key_line, f'duplicate key "{key}"')
            seen.add(key)
            self.skip_space()
            self.expect(":")
            node.value.append((key, key_line, self.value(depth + 1)))
            self.skip_space()
            if self.peek() == ",":
                self.at += 1
                continue
            self.expect("}")
            return node

    def array(self, depth: int) -> Node:
        node = Node("array", self.line, [])
        self.at += 1
        self.skip_space()
        if self.peek() == "]":
            self.at += 1
            return node
        while True:
            node.value.append(self.value(depth + 1))
            self.skip_space()
            if self.peek() == ",":
                self.at += 1
                continue
            self.expect("]")
            return node

    def string(self) -> str:
        start = self.at
        self.at += 1
        while True:
            char = self.peek()
            if not char or char == "\n":
                self.fail("unterminated string")
            if char == '"':
                self.at += 1
                return json.loads(self.text[start:self.at])  # Kaçışları standart çözücü açar
            if char == "\\":
                self.at += 1
            elif ord(char) < 0x20:
                self.fail("control character in a string")
            self.at += 1


def parse_json(text: str) -> Node:
    """Metni konumlu JSON ağacına çevirir; yorum, sondaki virgül, tekrar eden anahtar hatadır."""
    reader = _Reader(text)
    try:
        root = reader.value(0)
    except json.JSONDecodeError:
        reader.fail("invalid escape in a string")
    reader.skip_space()
    if reader.at != len(text):
        reader.fail("unexpected text after the JSON value")
    return root


# --- Doğrulama ----------------------------------------------------------------------------------------------------

def _number(node: Node, what: str) -> float:
    if node.kind != "number":
        raise TuningError(node.line, f"{what} must be a number")
    return float(node.value)


def _decode(node: Node, type_name: str, what: str):
    """Tek değer: float, int, bool, (r, g, b, a), (x, y) ya da (min, max)."""
    if type_name == "float":
        return _number(node, what)
    if type_name == "int":
        if node.kind != "number" or not INTEGER.match(node.value):
            raise TuningError(node.line, f"{what} must be an integer")
        value = int(node.value)
        if not INT_MIN <= value <= INT_MAX:
            raise TuningError(node.line, f"{what} does not fit a 32-bit int")
        return value
    if type_name == "bool":
        if node.kind != "bool":
            raise TuningError(node.line, f"{what} must be true or false")
        return node.value
    if type_name == "color":
        if node.kind != "string" or not COLOR.match(node.value):
            raise TuningError(node.line, f'{what} must be a colour "#RRGGBB" or "#RRGGBBAA"')
        digits = node.value[1:] + ("FF" if len(node.value) == 7 else "")
        return tuple(int(digits[i:i + 2], 16) for i in range(0, 8, 2))
    if type_name == "vec2":
        if node.kind != "array" or len(node.value) != 2:
            raise TuningError(node.line, f"{what} must be [x, y]")
        return tuple(_number(item, what) for item in node.value)
    if node.kind != "object" or sorted(key for key, _, _ in node.value) != sorted(RANGE_KEYS):
        raise TuningError(node.line, f'{what} must be {{ "min": a, "max": b }}')
    ends = {key: _number(item, what) for key, _, item in node.value}
    if ends["min"] > ends["max"]:
        raise TuningError(node.line, f"{what} has min greater than max")
    return ends["min"], ends["max"]


def _numbers(type_name: str, value) -> list[float]:
    if type_name in ("float", "int"):
        return [value]
    return list(value) if type_name in ("vec2", "range") else []


def _checked(node: Node, spec: Spec, what: str):
    """Değeri tipine göre çözer ve sınırlarını denetler; liste ise her elemanı."""
    def one(item: Node, label: str):
        value = _decode(item, spec.type, label)
        for number in _numbers(spec.type, value):
            if (spec.min is not None and number < spec.min) or (spec.max is not None and number > spec.max):
                raise TuningError(item.line, f"{label} {_shown(number)} is outside {_shown(spec.min)}..{_shown(spec.max)}")
        return value

    if not spec.is_list:
        return one(node, what)
    if node.kind != "array" or not node.value:
        raise TuningError(node.line, f"{what} must be a non-empty list")
    return [one(item, f"{what}[{index}]") for index, item in enumerate(node.value)]


def _shown(number: float | None) -> str:
    if number is None:
        return "*"
    return str(int(number)) if float(number).is_integer() else repr(number)


def _check_keys(node: Node, allowed: tuple[str, ...], where: str):
    for key, line, _ in node.value:
        if key not in allowed:
            raise TuningError(line, f'{where} has unknown key "{key}" (allowed: {", ".join(allowed)})')


def _check_name(name: str, line: int, what: str):
    if not FIELD_NAME.match(name):
        raise TuningError(line, f'{what} "{name}" must be camelCase: a lowercase letter, then letters and digits')
    if name in CPP_KEYWORDS:
        raise TuningError(line, f'{what} "{name}" is a C++ keyword')


def _spec(node: Node, type_node: Node | None, where: str, inferred: str | None = None) -> Spec:
    if type_node is None:
        if inferred is None:
            raise TuningError(node.line, f'{where} needs "type"')
        spec = Spec(inferred, False)
    else:
        if type_node.kind != "string" or not TYPE_NAME.match(type_node.value):
            types = ", ".join(SCALAR_TYPES)
            raise TuningError(type_node.line, f'{where} has an unknown type; use one of {types}, a list "T[]" or "table"')
        spec = Spec(type_node.value.removesuffix("[]"), type_node.value.endswith("[]"))
    keys = {key: item for key, _, item in node.value}
    for bound in ("min", "max"):
        if bound in keys:
            if spec.type not in BOUNDED_TYPES:
                raise TuningError(keys[bound].line, f'{where} of type {spec.text} cannot have "{bound}"')
            setattr(spec, bound, _number(keys[bound], f'"{bound}"'))
    if spec.min is not None and spec.max is not None and spec.min > spec.max:
        raise TuningError(keys["min"].line, f"{where} has min greater than max")
    return spec


def _inferred(value: Node | None) -> str | None:
    """`type` yazılmadığında değerden anlaşılan tip: sayı float'tır (3 kendiliğinden int olmaz), true/false bool."""
    if value is not None and value.kind == "number":
        return "float"
    return "bool" if value is not None and value.kind == "bool" else None


def _value_field(name: str, line: int, node: Node) -> Field:
    where = f'"{name}"'
    _check_keys(node, FIELD_KEYS, where)
    keys = {key: item for key, _, item in node.value}
    if "value" not in keys:
        raise TuningError(node.line, f'{where} needs "value"')
    spec = _spec(node, keys.get("type"), where, _inferred(keys["value"]))
    return Field(name, line, spec, _checked(keys["value"], spec, f"{where} value"))


def _table_field(name: str, line: int, node: Node) -> Field:
    where = f'table "{name}"'
    _check_keys(node, TABLE_KEYS, where)
    keys = {key: item for key, _, item in node.value}
    for required in ("columns", "rows"):
        if required not in keys:
            raise TuningError(node.line, f'{where} needs "{required}"')
    columns_node, rows_node = keys["columns"], keys["rows"]
    if columns_node.kind != "object" or not columns_node.value:
        raise TuningError(columns_node.line, f'{where} "columns" must be a non-empty object')
    result = Field(name, line)
    for column, column_line, column_node in columns_node.value:
        _check_name(column, column_line, "column")
        if column_node.kind != "object":
            raise TuningError(column_node.line, f'column "{column}" must be an object')
        _check_keys(column_node, COLUMN_KEYS, f'column "{column}"')
        type_node = next((item for key, _, item in column_node.value if key == "type"), None)
        result.columns.append((column, _spec(column_node, type_node, f'column "{column}"')))
    if rows_node.kind != "array" or not rows_node.value:
        raise TuningError(rows_node.line, f'{where} "rows" must be a non-empty list')
    lengths: dict[str, int] = {}
    for index, row_node in enumerate(rows_node.value):
        if row_node.kind != "object":
            raise TuningError(row_node.line, f"{where} row {index} must be an object")
        cells = {key: (key_line, item) for key, key_line, item in row_node.value}
        for key, (key_line, _) in cells.items():
            if key not in dict(result.columns):
                raise TuningError(key_line, f'{where} row {index} has unknown column "{key}"')
        row = {}
        for column, spec in result.columns:
            if column not in cells:
                raise TuningError(row_node.line, f'{where} row {index} is missing column "{column}"')
            cell = cells[column][1]
            row[column] = _checked(cell, spec, f'row {index} "{column}"')
            if spec.is_list:
                expected = lengths.setdefault(column, len(row[column]))
                if len(row[column]) != expected:
                    raise TuningError(cell.line, f'row {index} "{column}" has {len(row[column])} values; '
                                                 f"row 0 has {expected}")
        result.rows.append(row)
    return result


def parse_fields(root: Node) -> list[Field]:
    """Doğrulanmış alanlar, dosyadaki sırayla. İlk hatada TuningError."""
    if root.kind != "object":
        raise TuningError(root.line, "a tuning file is a JSON object of fields")
    if not root.value:
        raise TuningError(root.line, "a tuning file needs at least one field")
    fields = []
    for name, line, node in root.value:
        _check_name(name, line, "field")
        if node.kind != "object":
            raise TuningError(node.line, f'"{name}" must be an object such as {{ "value": 1.0 }}')
        type_node = next((item for key, _, item in node.value if key == "type"), None)
        is_table = type_node is not None and type_node.kind == "string" and type_node.value == "table"
        fields.append(_table_field(name, line, node) if is_table else _value_field(name, line, node))
    return fields


def read_fields(text: str) -> list[Field]:
    return parse_fields(parse_json(text))
