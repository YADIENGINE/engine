"""Input map files (Docs/InputMap.md): shared cases (the C++ side runs them too), the header, the canonical layout."""

import json
import re
import tempfile
import unittest
from pathlib import Path

from Tools.tuning import inputmap
from Tools.tuning.check import check_tuning
from Tools.tuning.document import TuningError

CASES = Path(__file__).resolve().parent / "InputMapCases"
ENGINE = Path(__file__).resolve().parents[2]


class SharedCaseTests(unittest.TestCase):
    def test_cases_match_the_expected_error_line(self):
        cases = json.loads((CASES / "cases.json").read_text(encoding="utf-8"))["cases"]
        self.assertTrue(cases)
        for case in cases:
            with self.subTest(case["file"]):
                text = (CASES / case["file"]).read_text(encoding="utf-8")
                if case["line"] is None:
                    inputmap.read_input_map(text)
                    continue
                with self.assertRaises(TuningError) as raised:
                    inputmap.read_input_map(text)
                self.assertEqual(raised.exception.line, case["line"], raised.exception.text)


class KeyNameTests(unittest.TestCase):
    def test_the_cpp_name_table_follows_the_enum(self):
        # C++ resolves the names the header holds; its table (InputMap.cpp) must list the enum in order.
        text = (ENGINE / "Core" / "Input" / "Internals" / "InputMap.cpp").read_text(encoding="utf-8")
        table = re.search(r"KeyNames\{(.*?)\};", text, re.S).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', table), inputmap.key_names())


class LayoutTests(unittest.TestCase):
    def test_canonical_orders_bindings_by_button(self):
        text = '{"players": [{"Jump": ["Space"], "Left": ["A"]}, {}], "buttons": ["Left", "Jump"]}'
        self.assertEqual(inputmap.canonical(text), '{\n  "buttons": ["Left", "Jump"],\n  "axes": [],\n  "players": [\n'
                                                   '    { "Left": ["A"], "Jump": ["Space"] },\n    {}\n  ]\n}\n')
        self.assertEqual(inputmap.canonical(inputmap.canonical(text)), inputmap.canonical(text))


class HeaderTests(unittest.TestCase):
    def test_header_carries_enums_names_and_keys(self):
        text = (CASES / "valid.json").read_text(encoding="utf-8")
        header = inputmap.render(inputmap.read_input_map(text), "ArenaInput", "Game/Simulation/Arena.input.json")
        self.assertIn("enum class Button : int { Left, Right, Jump };", header)
        self.assertIn("enum class Axis : int { MoveX };", header)
        self.assertIn('ButtonNames{"Left", "Right", "Jump"}', header)
        self.assertIn("static constexpr int Players = 2;", header)
        self.assertIn('Key{1, Button::Jump, "RightControl"},', header)
        self.assertIn('InputSource = u8"Game/Simulation/Arena.input.json"', header)

    def test_struct_name_comes_from_the_file_name(self):
        self.assertEqual(inputmap.struct_name(Path("Arena.input.json")), "ArenaInput")
        with self.assertRaises(TuningError):
            inputmap.struct_name(Path("arena.input.json"))


class CheckTests(unittest.TestCase):
    def test_check_reports_and_fixes_input_maps(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            path = root / "Game" / "Pad.input.json"
            path.parent.mkdir()
            path.write_text('{"buttons": ["Fire"], "players": [{"Fire": ["Space"]}]}', encoding="utf-8")
            problems, fixed = check_tuning(root, fix=False)
            self.assertEqual(problems, ["Game/Pad.input.json: not in the canonical layout (fix: check --fix)"])
            problems, fixed = check_tuning(root, fix=True)
            self.assertEqual((problems, fixed), ([], ["Game/Pad.input.json"]))


if __name__ == "__main__":
    unittest.main()
