"""Tuning dosyasının kanonik yazımı (Docs/Tuning.md → Yazma ve eşzamanlılık).

2 boşluk girinti; alanlar dosyadaki sırayla, her biri kendi satırında; alan içindeki anahtarlar sabit sırada
(type, value, min, max; tablo: type, columns, rows); tablo satırları sütun sırasıyla, satır başına bir. Sayılar
yazıldıkları gibi kalır. Editör tek değer değiştirirken aynı satır içi biçimi kullanır (Core/Tuning/TuningFile.cpp).
"""

import json

from .document import COLUMN_KEYS, FIELD_KEYS, RANGE_KEYS, TABLE_KEYS, Node, parse_fields, parse_json


def _key(text: str) -> str:
    return json.dumps(text, ensure_ascii=False)


def _ordered(node: Node, order: tuple[str, ...]) -> list[tuple[str, Node]]:
    pairs = [(key, item) for key, _, item in node.value]
    return sorted(pairs, key=lambda pair: order.index(pair[0]) if pair[0] in order else len(order))


def _inline(node: Node, order: tuple[str, ...] = ()) -> str:
    if node.kind == "object":
        if not node.value:
            return "{}"
        pairs = _ordered(node, order or RANGE_KEYS)
        return "{ " + ", ".join(f"{_key(key)}: {_inline(item)}" for key, item in pairs) + " }"
    if node.kind == "array":
        return "[" + ", ".join(_inline(item) for item in node.value) + "]"
    if node.kind == "string":
        return _key(node.value)
    if node.kind == "number":
        return node.value
    return "null" if node.kind == "null" else ("true" if node.value else "false")


def _block(lines: list[str], indent: str, items: list[str], closing: str):
    for index, item in enumerate(items):
        lines.append(indent + item + ("," if index < len(items) - 1 else ""))
    lines.append(indent[:-2] + closing)


def canonical(text: str) -> str:
    """Geçerli bir tuning dosyasının kanonik metni; geçersizse TuningError."""
    root = parse_json(text)
    fields = parse_fields(root)
    lines = ["{"]
    for index, ((name, _, node), field) in enumerate(zip(root.value, fields)):
        comma = "," if index < len(fields) - 1 else ""
        if not field.is_table:
            lines.append(f"  {_key(name)}: {_inline(node, FIELD_KEYS)}{comma}")
            continue
        parts = dict(_ordered(node, TABLE_KEYS))
        column_order = tuple(column for column, _ in field.columns)
        lines += [f"  {_key(name)}: {{", '    "type": "table",', '    "columns": {']
        columns = [f"{_key(key)}: {_inline(item, COLUMN_KEYS)}" for key, _, item in parts["columns"].value]
        _block(lines, "      ", columns, "},")
        lines.append('    "rows": [')
        _block(lines, "      ", [_inline(row, column_order) for row in parts["rows"].value], "]")
        lines.append("  }" + comma)
    return "\n".join(lines + ["}", ""])
