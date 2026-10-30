"""Tuning dosyaları: ortak biçim örnekleri (C++ çalışma zamanı da koşar), üretilen header, kanonik yazım, `check`."""

import json
import tempfile
import unittest
from pathlib import Path

from Tools.tuning.check import check_tuning
from Tools.tuning.document import TuningError, read_fields
from Tools.tuning.header import generate, render, struct_name
from Tools.tuning.layout import canonical

CASES = Path(__file__).resolve().parent / "TuningCases"

SAMPLE = """{
  "glowRadius": { "value": 12, "min": 0, "max": 40 },
  "count": { "type": "int", "value": 3 },
  "tint": { "type": "color", "value": "#FF4080" },
  "spread": { "type": "range", "value": { "min": 1, "max": 3 } },
  "odds": {
    "type": "table",
    "columns": {
      "from": { "type": "int", "min": 1 },
      "chances": { "type": "float[]", "min": 0, "max": 100 }
    },
    "rows": [
      { "from": 2, "chances": [70, 30] },
      { "from": 5, "chances": [55, 45] }
    ]
  }
}
"""


class SharedCaseTests(unittest.TestCase):
    def test_cases_match_the_expected_error_line(self):
        cases = json.loads((CASES / "cases.json").read_text(encoding="utf-8"))["cases"]
        self.assertTrue(cases)
        for case in cases:
            with self.subTest(case["file"]):
                text = (CASES / case["file"]).read_text(encoding="utf-8")
                if case["line"] is None:
                    read_fields(text)
                    continue
                with self.assertRaises(TuningError) as raised:
                    read_fields(text)
                self.assertEqual(raised.exception.line, case["line"], raised.exception.text)


class DocumentTests(unittest.TestCase):
    def test_number_without_type_is_float(self):
        field = read_fields('{"a": {"value": 3}}')[0]
        self.assertEqual((field.spec.type, field.value), ("float", 3.0))

    def test_table_rows_follow_column_order(self):
        table = read_fields(SAMPLE)[-1]
        self.assertTrue(table.is_table)
        self.assertEqual([name for name, _ in table.columns], ["from", "chances"])
        self.assertEqual(table.rows[1], {"from": 5, "chances": [55.0, 45.0]})


class HeaderTests(unittest.TestCase):
    def test_struct_name_comes_from_the_file_name(self):
        self.assertEqual(struct_name(Path("Effects/Glow.tuning.json")), "GlowTuning")
        for bad in ("glow.tuning.json", "Glow.json", "Glow-Fx.tuning.json"):
            with self.subTest(bad), self.assertRaises(TuningError):
                struct_name(Path(bad))

    def test_members_carry_the_file_values(self):
        text = render(read_fields(SAMPLE), "SampleTuning", Path("Sample.tuning.json"))
        for expected in ("struct SampleTuning {", "float glowRadius = 12.0f;", "int count = 3;",
                         "Color4 tint = Color4{255, 64, 128, 255};", "FloatRange spread = FloatRange{1.0f, 3.0f};",
                         "struct OddsRow {", "std::array<float, 2> chances{};", "std::array<OddsRow, 2> odds{",
                         "OddsRow{5, {55.0f, 45.0f}},", 'visitor.Table("odds", odds);', "#if ENGINE_TUNING_LIVE",
                         '#include "Tuning/Tuning.h"', '#include "Utilities/Color4.h"'):
            self.assertIn(expected, text)
        self.assertNotIn("Float2", text)

    def test_unchanged_header_is_not_rewritten(self):
        with tempfile.TemporaryDirectory(prefix="EngineTuning-") as directory:
            source, header = Path(directory) / "Sample.tuning.json", Path(directory) / "out" / "Sample.tuning.h"
            source.write_text(SAMPLE, encoding="utf-8")
            self.assertTrue(generate(source, header))
            self.assertFalse(generate(source, header))
            source.write_text(SAMPLE.replace('"value": 3', '"value": 4'), encoding="utf-8")
            self.assertTrue(generate(source, header))
            self.assertIn("int count = 4;", header.read_text(encoding="utf-8"))


class LayoutTests(unittest.TestCase):
    def test_canonical_is_stable(self):
        self.assertEqual(canonical(SAMPLE), SAMPLE)

    def test_layout_is_normalised_and_numbers_are_kept(self):
        messy = ('{"a":{"max":40,"value":12.50,"min":0},"t":{"rows":[{"y":1,"x":2}],"type":"table",'
                 '"columns":{"x":{"type":"int"},"y":{"type":"int"}}}}')
        expected = ('{\n  "a": { "value": 12.50, "min": 0, "max": 40 },\n  "t": {\n    "type": "table",\n'
                    '    "columns": {\n      "x": { "type": "int" },\n      "y": { "type": "int" }\n    },\n'
                    '    "rows": [\n      { "x": 2, "y": 1 }\n    ]\n  }\n}\n')
        self.assertEqual(canonical(messy), expected)

    def test_invalid_file_has_no_canonical_form(self):
        with self.assertRaises(TuningError):
            canonical('{"a": {"value": 1},}')


class CheckTests(unittest.TestCase):
    def setUp(self):
        self._directory = tempfile.TemporaryDirectory(prefix="EngineTuningCheck-")
        self.root = Path(self._directory.name)

    def tearDown(self):
        self._directory.cleanup()

    def test_reports_and_fixes(self):
        (self.root / "Game").mkdir()
        good = self.root / "Game" / "Good.tuning.json"
        messy = self.root / "Game" / "Messy.tuning.json"
        broken = self.root / "Game" / "Broken.tuning.json"
        good.write_text(SAMPLE, encoding="utf-8")
        messy.write_text('{"a": {"value": 1}}', encoding="utf-8")
        broken.write_text('{\n  "a": { "value": true, "min": 0 }\n}\n', encoding="utf-8")

        problems, fixed = check_tuning(self.root, fix=False)
        self.assertEqual(fixed, [])
        self.assertIn("Game/Broken.tuning.json:2: \"a\" of type bool cannot have \"min\"", problems)
        self.assertTrue(any(problem.startswith("Game/Messy.tuning.json: not in the canonical layout")
                            for problem in problems))
        self.assertEqual(len(problems), 2)

        problems, fixed = check_tuning(self.root, fix=True)
        self.assertEqual(fixed, ["Game/Messy.tuning.json"])
        self.assertEqual(len(problems), 1)
        self.assertEqual(messy.read_text(encoding="utf-8"), '{\n  "a": { "value": 1 }\n}\n')

    def test_build_trees_are_skipped(self):
        build = self.root / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text("", encoding="utf-8")
        (build / "Old.tuning.json").write_text("{", encoding="utf-8")
        self.assertEqual(check_tuning(self.root, fix=False), ([], []))


if __name__ == "__main__":
    unittest.main()
